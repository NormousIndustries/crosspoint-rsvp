#include "RsvpReaderActivity.h"

#include <HalGPIO.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <algorithm>

#include "EpubReaderUtils.h"
#include "components/UITheme.h"

void RsvpReaderActivity::onEnter() {
  Activity::onEnter();
  RenderLock lock;
  previousOrientation = renderer.getOrientation();
  previousRenderMode = renderer.getRenderMode();
  previousInverted = display.isInverted();
  if (!gpio.deviceIsX3() || !entry.hasVisibleTextOffset) {
    LOG_ERR("RSVP1", "RSVP requires X3 and a visible-text position");
    ActivityResult cancelled;
    cancelled.isCancelled = true;
    setResult(std::move(cancelled));
    finish();
    return;
  }
  renderer.setOrientation(GfxRenderer::LandscapeCounterClockwise);
  renderer.setRenderMode(GfxRenderer::BW);
  display.setInverted(false);
  loadingChapter = entry.spineIndex;
  loadingOffset = entry.visibleTextOffset;
  layout = UITheme::getInstance().rsvpLayout(renderer);
  entryHeap = ESP.getFreeHeap();
  LOG_INF("RSVP2", "LAYOUT focus_x=%d baseline=%d word=%d,%d,%d,%d marker_tips=%d,%d controls_y=%d row=%d",
          layout.focusX, layout.baseline, layout.word.x, layout.word.y, layout.word.width, layout.word.height,
          layout.upperTip, layout.lowerTip, layout.controlsY, layout.rowHeight);
  sampleHeap();
  LOG_INF("RSVP1", "ENTER book=%s chapter=%d start_offset=%u activity_bytes=%u parser_bytes=%u heap=%u min_heap=%u",
          epub->getPath().c_str(), entry.spineIndex, entry.visibleTextOffset, static_cast<unsigned>(sizeof(*this)),
          static_cast<unsigned>(sizeof(ChapterHtmlSlimParser)), ESP.getFreeHeap(), ESP.getMinFreeHeap());
  requestUpdate();
}

bool RsvpReaderActivity::preventAutoSleep() {
  const auto current = state.load();
  return current == State::Playing || current == State::Displaying || current == State::Loading;
}

void RsvpReaderActivity::sampleHeap() {
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  minimumHeap = std::min(minimumHeap, static_cast<uint32_t>(heap_caps_get_free_size(caps)));
  minimumBlock = std::min(minimumBlock, static_cast<uint32_t>(heap_caps_get_largest_free_block(caps)));
}

void RsvpReaderActivity::reportSummary() {
  if (!cadence.wordsPresented) return;
  LOG_INF("RSVP1",
          "SUMMARY words_presented=%u elapsed_us=%lld selected_wpm=%u effective_wpm=%u deadline_misses=%u "
          "chapter_changes=%u service_us=%lld heap=%u min_heap=%u min_block=%u",
          cadence.wordsPresented, cadence.elapsedUs(), cadence.wpm(), cadence.effectiveWpm(), cadence.deadlineMisses,
          chapterChanges, cadence.serviceUs, ESP.getFreeHeap(), minimumHeap, minimumBlock);
  LOG_INF("RSVP1",
          "SMART normal=%u long=%u comma=%u clause=%u sentence=%u paragraph=%u additional_us=%lld "
          "avg_service_us=%lld system_min_heap=%u",
          cadence.timingCounts[0], cadence.timingCounts[1], cadence.timingCounts[2], cadence.timingCounts[3],
          cadence.timingCounts[4], cadence.timingCounts[5], cadence.additionalUs,
          cadence.serviceUs / cadence.wordsPresented, ESP.getMinFreeHeap());
  cadence.resetSummary();
  chapterChanges = 0;
}

void RsvpReaderActivity::finishAtCurrentPosition() {
  state.store(State::Exiting);
  job = Job::None;
  if (const auto* token = history.current()) {
    ProgressChangeResult position;
    position.spineIndex = token->spineIndex;
    position.visibleTextOffset = token->startOffset;
    position.hasVisibleTextOffset = true;
    setResult(std::move(position));
  } else {
    ActivityResult cancelled;
    cancelled.isCancelled = true;
    setResult(std::move(cancelled));
  }
  finish();
}

void RsvpReaderActivity::loop() {
  using Button = MappedInputManager::Button;
  uint8_t incoming = 0;
  if (mappedInput.wasReleased(Button::Confirm)) incoming |= TOGGLE;
  if (mappedInput.wasReleased(Button::Left)) incoming |= PREVIOUS;
  if (mappedInput.wasReleased(Button::Right)) incoming |= NEXT;
  // X3 landscape CCW: the top speed button reports Down, the bottom reports Up.
  if (mappedInput.wasReleased(Button::Down)) incoming |= FASTER;
  if (mappedInput.wasReleased(Button::Up)) incoming |= SLOWER;
  if (mappedInput.wasReleased(Button::Back)) incoming |= EXIT;
  controls.fetch_or(incoming);
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (controls.load() & EXIT) {
    controls.store(0);
    finishAtCurrentPosition();
    return;
  }
  if (state.load() == State::Loading) {
    requestUpdate();
    return;  // Retain controls through bounded chapter-loading ticks.
  }
  if (state.load() == State::Exiting) return;
  const uint8_t pending = controls.exchange(0);
  if (pending & (PREVIOUS | NEXT)) {
    state.store(State::Paused);
    reportSummary();
    job = (pending & PREVIOUS) && history.current() ? Job::Previous : Job::Next;
  } else if (pending & TOGGLE) {
    if (state.load() == State::Playing) {
      state.store(State::Paused);
      reportSummary();
    } else if (history.current() && !sourceFailed) {
      state.store(State::Playing);
      cadence.resetSummary();
    }
    if (history.current()) job = Job::Current;
  }
  if (pending & (FASTER | SLOWER)) {
    reportSummary();
    if (pending & FASTER) cadence.faster();
    if (pending & SLOWER) cadence.slower();
    cadence.rebase(esp_timer_get_time());
    if (state.load() == State::Paused && job == Job::None) job = Job::Current;
  }
  if (state.load() == State::Playing && job == Job::None && cadence.due(esp_timer_get_time())) {
    job = Job::TimedNext;
  }
  if (job != Job::None) requestUpdate();
}

bool RsvpReaderActivity::drawFrame(const RsvpToken* token, bool paused) {
  const int64_t constructionStart = esp_timer_get_time();
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.clearScreen();
  bool oversized = false;
  rsvp::Placement placement;
  if (token && !paused) {
    const auto orp = rsvp::findOrp(token->text);
    placement = rsvp::place(token->text, orp, layout.focusX, layout.word.x, layout.word.x + layout.word.width,
                            layout.word.y - layout.baseline, layout.word.y + layout.word.height - layout.baseline);
    oversized = !placement.fits;
    ++fontPresentations[static_cast<uint8_t>(placement.size)];
    if (oversized) ++oversizedWords;
    if (!oversized) UITheme::getInstance().drawRsvpWord(renderer, layout, token->text, placement);
  }
  contextFrame = paused || oversized;
  if (!paused && !oversized) return false;
  if (token) {
    if (contextSpine != token->spineIndex || contextOffset != token->startOffset) {
      if (!source.context(*token, context)) {
        LOG_ERR("RSVP2", "Context fallback chapter=%d offset=%u", token->spineIndex, token->startOffset);
      }
      contextSpine = token->spineIndex;
      contextOffset = token->startOffset;
    }
    UITheme::getInstance().drawRsvpContext(renderer, layout, context, contextPlacement);
  }
  const char* statusText = message == StrId::STR_LOADING           ? tr(STR_LOADING)
                           : message == StrId::STR_PAGE_LOAD_ERROR ? tr(STR_PAGE_LOAD_ERROR)
                           : message == StrId::STR_END_OF_BOOK     ? tr(STR_END_OF_BOOK)
                                                                   : tr(STR_RSVP_PAUSED);
  UITheme::getInstance().drawRsvpPaused(renderer, layout, statusText, cadence.wpm(), oversized, mappedInput, {});
  contextBuildUs = esp_timer_get_time() - constructionStart;
  return oversized;
}

void RsvpReaderActivity::failSource() {
  LOG_ERR("RSVP1", "Stopping at last completed token: chapter=%d", source.chapter());
  source.close();
  sourceFailed = true;
  state.store(State::Paused);
  message = StrId::STR_PAGE_LOAD_ERROR;
  job = Job::Current;
  reportSummary();
  requestUpdate();
}

void RsvpReaderActivity::loadSome() {
  if (loadingChrome) {
    message = StrId::STR_LOADING;
    drawFrame(history.current(), true);
    const auto completion = backend.submit();
    loadingChrome = false;
    if (!completion.success) {
      failSource();
      job = Job::None;
      return;
    }
  }
  if (needsChapter) {
    if (!source.beginChapter(loadingChapter, loadingOffset, renderer)) {
      failSource();
      return;
    }
    needsChapter = false;
  }
  const auto result = source.buildSome();
  sampleHeap();
  if (result == RsvpEpubSource::BuildResult::Error) {
    failSource();
    return;
  }
  if (result == RsvpEpubSource::BuildResult::Ready) {
    if (afterLoading == State::Playing) cadence.resume(esp_timer_get_time());
    state.store(afterLoading);
    message = StrId::STR_RSVP_PAUSED;
    job = Job::Next;
  }
  requestUpdate();
}

void RsvpReaderActivity::render(RenderLock&&) {
  if ((controls.load() & EXIT) || state.load() == State::Exiting) return;
  if (state.load() == State::Loading) {
    loadSome();
    return;
  }
  const Job presentation = job;
  job = Job::None;
  if (presentation == Job::None) return;
  // A queued control takes precedence over automatic advancement.
  if (presentation == Job::TimedNext && controls.load()) return;
  const RsvpToken* token = history.current();
  bool replay = false;
  bool previous = false;
  if (presentation == Job::Previous) {
    token = history.peekPrevious();
    previous = token != nullptr;
    if (!token) token = history.current();
  } else if (presentation == Job::Next || presentation == Job::TimedNext) {
    token = history.peekNext();
    replay = token != nullptr;
    if (!token && !sourceFailed && !endReached) {
      if (!stagedAvailable) {
        const auto result = source.next(staged);
        if (result == RsvpEpubSource::ReadResult::Error) {
          failSource();
          return;
        }
        if (result == RsvpEpubSource::ReadResult::ChapterEnd) {
          if (source.hasNextChapter()) {
            cadence.suspend(esp_timer_get_time());
            afterLoading = state.load();
            state.store(State::Loading);
            needsChapter = true;
            loadingChapter = source.chapter() + 1;
            loadingOffset = 0;
            loadingChrome = true;
            ++chapterChanges;
            requestUpdate();
            return;
          }
          endReached = true;
          state.store(State::Paused);
          message = StrId::STR_END_OF_BOOK;
          reportSummary();
        } else {
          stagedAvailable = true;
        }
      }
      if (stagedAvailable) token = &staged;
    }
    if (!token) {
      token = history.current();
      if (endReached) {
        state.store(State::Paused);
        message = StrId::STR_END_OF_BOOK;
        reportSummary();
      }
    }
  }
  State resumeState = state.load();
  bool playing = resumeState == State::Playing;
  if (!sourceFailed && (previous || replay)) {
    message = StrId::STR_RSVP_PAUSED;
  } else if (!sourceFailed && endReached) {
    message = StrId::STR_END_OF_BOOK;
  }
  state.store(State::Displaying);
  if (drawFrame(token, !playing)) {
    resumeState = State::Paused;
    playing = false;
    reportSummary();
  }
  const int64_t start = esp_timer_get_time();
  const auto completion = backend.submit();
  const int64_t end = esp_timer_get_time();
  if (contextFrame) {
    LOG_INF("RSVP2",
            "CONTEXT chapter=%d offset=%u words=%u selected=%u bytes=%u lines=%u excerpt=%u,%u "
            "construct_us=%lld transition_us=%lld heap=%u min_heap=%u block=%u",
            token ? token->spineIndex : -1, token ? token->startOffset : 0, context.count, context.selected,
            context.used, contextPlacement.lines, contextPlacement.leading, contextPlacement.trailing, contextBuildUs,
            contextBuildUs + end - start, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  if (!completion.success) {
    sourceFailed = true;
    message = StrId::STR_PAGE_LOAD_ERROR;
    state.store(State::Paused);
    reportSummary();
    return;  // Never commit a token whose presentation failed.
  }
  if (token == &staged) {
    history.append(staged);
    stagedAvailable = false;
  } else if (replay) {
    history.replayNext();
  } else if (previous) {
    history.previous();
  }
  if (playing && token) cadence.completed(start, end, token->timing);
  state.store(resumeState);
  sampleHeap();
}

void RsvpReaderActivity::onExit() {
  state.store(State::Exiting);
  reportSummary();
  source.close();
  LOG_INF("RSVP2",
          "VISUAL_SUMMARY normal=%u long=%u extra_long=%u oversized=%u entry_heap=%u exit_heap=%u "
          "min_heap=%u min_block=%u",
          fontPresentations[0], fontPresentations[1], fontPresentations[2], oversizedWords, entryHeap,
          ESP.getFreeHeap(), minimumHeap, minimumBlock);
  // Also protects paused auto-sleep/global navigation, which do not deliver a child result.
  if (const auto* token = history.current()) {
    if (!EpubReaderUtils::saveProgress(*epub, token->spineIndex, 0, 0, token->startOffset)) {
      LOG_ERR("RSVP1", "Could not persist final content position");
    }
    LOG_INF("RSVP1", "EXIT chapter=%d text_offset=%u heap=%u min_heap=%u truncations=%u", token->spineIndex,
            token->startOffset, ESP.getFreeHeap(), ESP.getMinFreeHeap(), source.truncations());
  }
  renderer.setOrientation(previousOrientation);
  renderer.setRenderMode(previousRenderMode);
  display.setInverted(previousInverted);
  Activity::onExit();
}
