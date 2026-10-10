#include "RsvpDisplayTestActivity.h"

#include <BoardConfig.h>
#include <HalDisplay.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "components/UIScale.h"
#include "components/UITheme.h"

void RsvpDisplayTestActivity::onEnter() {
  Activity::onEnter();
  RenderLock lock;
  previousOrientation = renderer.getOrientation();
  previousRenderMode = renderer.getRenderMode();
  if (!gpio.deviceIsX3()) {
    LOG_ERR("RSVP", "Display test requires X3 hardware");
    finish();
    return;
  }
  renderer.setOrientation(GfxRenderer::LandscapeCounterClockwise);
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const auto& metrics = UITheme::getInstance().getMetrics();
  safeRect = Rect(left, top, renderer.getScreenWidth() - left - right - metrics.buttonHintsHeight,
                  renderer.getScreenHeight() - top - bottom);
  const int height = renderer.getLineHeight(uiScaleSpec().titleFontId) * 2;
  const int padding = metrics.contentSidePadding + metrics.sideButtonHintsWidth;
  const int contentTop = safeRect.y + metrics.topPadding + metrics.headerHeight +
                         renderer.getLineHeight(uiScaleSpec().smallFontId) + metrics.verticalSpacing;
  wordRect = Rect(safeRect.x + padding, contentTop + (safeRect.y + safeRect.height - contentTop - height) / 2,
                  safeRect.width - padding * 2, height);
  if (!loadPassage()) {
    finish();
    return;
  }
  LOG_INF("RSVP", "v0.1A board=%s controller=%s variant=%u panel=%ux%u", BoardConfig::ACTIVE.name,
          BoardConfig::ACTIVE.displayController == BoardConfig::DisplayController::UC8253 ? "UC8253" : "UC8279",
          BoardConfig::ACTIVE.displayControllerVariant, display.getDisplayWidth(), display.getDisplayHeight());
  LOG_INF("RSVP", "activity_bytes=%u samples_bytes=%u words=%u path=blocking-HAL/full-frame FAST; SDK wait logs retained",
          static_cast<unsigned>(sizeof(*this)), static_cast<unsigned>(sizeof(samples)), WORD_COUNT);
  sampleHeap();
  requestUpdate();
}

bool RsvpDisplayTestActivity::loadPassage() {
  const char* passage = tr(STR_RSVP_TEST_PASSAGE);
  size_t offset = 0;
  uint16_t count = 0;
  while (passage[offset]) {
    while (passage[offset] == ' ') ++offset;
    if (!passage[offset]) break;
    if (count == WORD_COUNT || offset > UINT16_MAX) {
      LOG_ERR("RSVP", "Invalid test passage size");
      return false;
    }
    wordOffsets[count++] = static_cast<uint16_t>(offset);
    const size_t start = offset;
    while (passage[offset] && passage[offset] != ' ') ++offset;
    if (offset - start >= sizeof(wordBuffer)) {
      LOG_ERR("RSVP", "Test word exceeds buffer");
      return false;
    }
  }
  if (count != WORD_COUNT) {
    LOG_ERR("RSVP", "Expected %u words, got %u", WORD_COUNT, count);
    return false;
  }
  return true;
}

void RsvpDisplayTestActivity::drawWord(const uint16_t index) {
  const char* word = tr(STR_RSVP_TEST_PASSAGE) + wordOffsets[index];
  size_t length = 0;
  while (word[length] && word[length] != ' ') ++length;
  memcpy(wordBuffer, word, length);
  wordBuffer[length] = '\0';
  renderer.fillRect(wordRect.x, wordRect.y, wordRect.width, wordRect.height, false);
  UITheme::drawCenteredText(renderer, wordRect, uiScaleSpec().titleFontId,
                            wordRect.y + (wordRect.height - renderer.getLineHeight(uiScaleSpec().titleFontId)) / 2,
                            wordBuffer);
}

void RsvpDisplayTestActivity::onExit() {
  playing.store(false);
  displayJob = DisplayJob::None;
  printReport("exit");
  sampleHeap();
  LOG_INF("RSVP", "exit free_heap=%lu largest_block=%lu; activity freed after onExit",
          static_cast<unsigned long>(lastFreeHeap), static_cast<unsigned long>(lastLargestBlock));
  renderer.setOrientation(previousOrientation);
  renderer.setRenderMode(previousRenderMode);
  Activity::onExit();
}

void RsvpDisplayTestActivity::loop() {
  using Button = MappedInputManager::Button;
  // Stop requests bypass render contention; a committed synchronous call still completes.
  if (mappedInput.wasReleased(Button::Confirm)) pendingControls |= playing.exchange(false) ? PAUSE : PLAY;
  if (mappedInput.wasReleased(Button::Left)) pendingControls |= PREVIOUS;
  if (mappedInput.wasReleased(Button::Right)) pendingControls |= NEXT;
  if (mappedInput.wasReleased(Button::Up)) pendingControls |= FASTER;
  if (mappedInput.wasReleased(Button::Down)) pendingControls |= SLOWER;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    pendingControls |= EXIT;
  }
  if (pendingControls & (PAUSE | PREVIOUS | NEXT | FASTER | SLOWER | EXIT)) playing.store(false);

  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (!playing.load()) printReport(pendingControls & EXIT ? "exit" : "paused-or-complete");
  const uint8_t controls = pendingControls;
  pendingControls = 0;
  if (controls & EXIT) {
    displayJob = DisplayJob::None;
    finish();
    return;
  }
  if (controls & (PAUSE | PREVIOUS | NEXT | FASTER | SLOWER)) {
    displayJob = DisplayJob::Chrome;
    runNeedsBaseline = false;
    targetWord = displayedWord;
    if ((controls & PREVIOUS) && displayedWord > 0) --targetWord;
    if ((controls & NEXT) && targetWord + 1 < WORD_COUNT) ++targetWord;
    if (controls & FASTER) speedIndex = (speedIndex + 1) % 3;
    if (controls & SLOWER) speedIndex = (speedIndex + 2) % 3;
  } else if (controls & PLAY) {
    if (displayedWord + 1 == WORD_COUNT) targetWord = 0;
    else targetWord = displayedWord;
    sampleCount = 0;
    minimumFreeHeap = UINT32_MAX;
    minimumLargestBlock = UINT32_MAX;
    sampleHeap();
    runWpm = SPEEDS[speedIndex];
    runNeedsBaseline = true;
    reportPending = true;
    displayJob = DisplayJob::Chrome;
    playing.store(true);
  } else if (!playing.load() && displayJob == DisplayJob::Timed) {
    displayJob = DisplayJob::Chrome;
    targetWord = displayedWord;
  }
  if (playing.load() && !runNeedsBaseline && displayJob == DisplayJob::None && esp_timer_get_time() >= nextDeadline) {
    targetWord = displayedWord + 1;
    committedDeadline = nextDeadline;
    displayJob = DisplayJob::Timed;
  }
  if (displayJob != DisplayJob::None) requestUpdate();
}

void RsvpDisplayTestActivity::render(RenderLock&& lock) {
  const int64_t renderStart = esp_timer_get_time();
  const DisplayJob job = displayJob;
  if (job == DisplayJob::None) return;
  displayJob = DisplayJob::None;
  if (job == DisplayJob::Timed && !playing.load()) return;
  renderer.setRenderMode(GfxRenderer::BW);
  if (job == DisplayJob::Chrome) drawChrome();
  drawWord(targetWord);
  const int64_t rasterDone = esp_timer_get_time();
  if (job == DisplayJob::Timed && !playing.load()) return;
  const int64_t displayStart = esp_timer_get_time();
  // The HAL call includes the driver BUSY wait. No framebuffer reuse before return.
  display.displayBuffer(initialFrame ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  const int64_t displayReturn = esp_timer_get_time();
  initialFrame = false;
  displayedWord = targetWord;
  if (job == DisplayJob::Timed) {
    samples[sampleCount++] = {committedDeadline, renderStart, rasterDone, displayStart, displayReturn, displayedWord};
    // Rebase on actual start: an over-budget call makes the next word due as
    // soon as the display is available, without advancing across missed slots.
    nextDeadline = renderStart + 60000000LL / runWpm;
    if (displayedWord + 1 == WORD_COUNT) playing.store(false);
  } else if (runNeedsBaseline && playing.load()) {
    baselineStart = renderStart;
    baselineReturn = displayReturn;
    nextDeadline = displayReturn + 60000000LL / runWpm;
    runNeedsBaseline = false;
  }
  sampleHeap();
}

void RsvpDisplayTestActivity::drawChrome() {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect(safeRect.x, safeRect.y, safeRect.width, metrics.headerHeight), tr(STR_RSVP_DISPLAY_TEST));
  char status[96];
  if (playing.load()) {
    snprintf(status, sizeof(status), tr(STR_RSVP_TEST_PLAY_STATUS), tr(STR_RSVP_PLAYING), SPEEDS[speedIndex]);
  } else {
    snprintf(status, sizeof(status), tr(STR_RSVP_TEST_STATUS), tr(STR_RSVP_PAUSED), SPEEDS[speedIndex], targetWord + 1,
             WORD_COUNT);
  }
  UITheme::drawCenteredText(renderer, safeRect, uiScaleSpec().smallFontId,
                            safeRect.y + metrics.topPadding + metrics.headerHeight,
                            status);
  GUI.drawButtonHints(renderer, tr(STR_BACK), tr(STR_RSVP_PLAY_PAUSE), tr(STR_RSVP_PREVIOUS), tr(STR_RSVP_NEXT));
  GUI.drawSideButtonHints(renderer, tr(STR_RSVP_FASTER), tr(STR_RSVP_SLOWER));
}

void RsvpDisplayTestActivity::sampleHeap() {
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  lastFreeHeap = heap_caps_get_free_size(caps);
  lastLargestBlock = heap_caps_get_largest_free_block(caps);
  minimumFreeHeap = std::min(minimumFreeHeap, lastFreeHeap);
  minimumLargestBlock = std::min(minimumLargestBlock, lastLargestBlock);
}

void RsvpDisplayTestActivity::printReport(const char* reason) {
  if (!reportPending) return;
  reportPending = false;
  const auto bootLowWatermark = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const int64_t interval = 60000000LL / runWpm;
  uint32_t misses = 0;
  int64_t rasterSum = 0;
  int64_t displaySum = 0;
  int64_t serviceSum = 0;
  int64_t maxService = 0;
  int64_t minCadence = INT64_MAX;
  int64_t maxCadence = 0;
  int64_t maxLateness = 0;
  for (uint16_t i = 0; i < sampleCount; ++i) {
    const auto& sample = samples[i];
    rasterSum += sample.rasterDone - sample.renderStart;
    displaySum += sample.displayReturn - sample.displayStart;
    serviceSum += sample.displayReturn - sample.renderStart;
    maxService = std::max(maxService, sample.displayReturn - sample.renderStart);
    maxLateness = std::max(maxLateness, sample.renderStart - sample.deadline);
    if (sample.displayReturn > sample.deadline + interval) ++misses;
    if (i > 0) {
      const int64_t cadence = sample.displayReturn - samples[i - 1].displayReturn;
      minCadence = std::min(minCadence, cadence);
      maxCadence = std::max(maxCadence, cadence);
    }
  }
  const double delivered = sampleCount > 1 ? 60000000.0 * (sampleCount - 1) /
                                                (samples[sampleCount - 1].displayReturn - samples[0].displayReturn)
                                          : 0;
  const double overall = sampleCount ? 60000000.0 * sampleCount /
                                          (samples[sampleCount - 1].displayReturn - baselineReturn)
                                    : 0;
  LOG_INF("RSVP", "SUMMARY reason=%s selected_wpm=%u interval_us=%lld completed=%u missed=%lu delivered_wpm=%.2f overall_wpm=%.2f",
          reason, runWpm, interval, sampleCount, static_cast<unsigned long>(misses), delivered, overall);
  if (sampleCount) {
    LOG_INF("RSVP", "TIMING raster_mean_us=%lld display_mean_us=%lld service_mean_us=%lld service_max_us=%lld late_max_us=%lld",
            rasterSum / sampleCount, displaySum / sampleCount, serviceSum / sampleCount, maxService, maxLateness);
  }
  if (sampleCount > 1) {
    LOG_INF("RSVP", "CADENCE completion_min_us=%lld completion_max_us=%lld pairs=%u", minCadence, maxCadence,
            sampleCount - 1);
  }
  LOG_INF("RSVP", "HEAP min_free=%lu min_largest=%lu last_free=%lu last_largest=%lu (internal 8-bit, post-call samples)",
          static_cast<unsigned long>(minimumFreeHeap), static_cast<unsigned long>(minimumLargestBlock),
          static_cast<unsigned long>(lastFreeHeap), static_cast<unsigned long>(lastLargestBlock));
  LOG_INF("RSVP", "HEAP boot_low_watermark=%u (sum of region lows since boot, includes transient allocations)",
          static_cast<unsigned>(bootLowWatermark));
  LOG_INF("RSVP", "BASELINE render_start_us=%lld display_return_us=%lld; setup/manual refreshes excluded", baselineStart,
          baselineReturn);
  for (uint16_t i = 0; i < sampleCount; ++i) {
    const auto& sample = samples[i];
    LOG_INF("RSVP", "STAMP n=%u word=%u deadline_us=%lld start_us=%lld raster_done_us=%lld call_us=%lld return_us=%lld",
            i + 1, sample.word + 1, sample.deadline, sample.renderStart, sample.rasterDone, sample.displayStart,
            sample.displayReturn);
    LOG_INF("RSVP", "DELTA n=%u raster_us=%lld display_us=%lld service_us=%lld start_interval_us=%lld completion_interval_us=%lld late_us=%lld miss=%u",
            i + 1, sample.rasterDone - sample.renderStart, sample.displayReturn - sample.displayStart,
            sample.displayReturn - sample.renderStart,
            sample.renderStart - (i ? samples[i - 1].renderStart : baselineStart),
            sample.displayReturn - (i ? samples[i - 1].displayReturn : baselineReturn),
            std::max<int64_t>(0, sample.renderStart - sample.deadline), sample.displayReturn > sample.deadline + interval);
  }
  if (displayedWord + 1 == WORD_COUNT) {
    targetWord = displayedWord;
    displayJob = DisplayJob::Chrome;
  }
}
