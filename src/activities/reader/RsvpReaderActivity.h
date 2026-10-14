#pragma once

#include <CrossPointPosition.h>
#include <I18n.h>
#include <RsvpCadence.h>
#include <RsvpDisplayBackend.h>
#include <RsvpEpubSource.h>

#include <atomic>

#include "activities/Activity.h"
#include "components/RsvpReaderUi.h"

class RsvpReaderActivity final : public Activity {
 public:
  RsvpReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::shared_ptr<Epub> epub,
                     CrossPointPosition position, bool embeddedStyle)
      : Activity("RsvpReader", renderer, mappedInput),
        epub(std::move(epub)),
        source(this->epub, embeddedStyle),
        entry(position) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override;
  bool handleForcedRefresh() override { return true; }

 private:
  enum class State : uint8_t { Paused, Playing, Displaying, Loading, Exiting };
  enum class Job : uint8_t { None, Current, Next, Previous, TimedNext };
  enum Control : uint8_t { TOGGLE = 1, PREVIOUS = 2, NEXT = 4, FASTER = 8, SLOWER = 16, EXIT = 32 };
  std::shared_ptr<Epub> epub;
  RsvpEpubSource source;
  CrossPointPosition entry;
  RsvpHistory history;
  RsvpToken staged;
  bool stagedAvailable = false;
  RsvpDisplayBackend backend;
  RsvpCadence cadence;
  std::atomic<State> state{State::Loading};
  std::atomic<uint8_t> controls{0};
  // All non-atomic members are protected by ActivityManager's RenderLock.
  State afterLoading = State::Paused;
  Job job = Job::None;
  bool needsChapter = true;
  int loadingChapter = 0;
  uint32_t loadingOffset = 0;
  bool loadingChrome = true;
  bool sourceFailed = false;
  bool endReached = false;
  StrId message = StrId::STR_RSVP_PAUSED;
  uint32_t chapterChanges = 0;
  uint32_t minimumHeap = UINT32_MAX;
  uint32_t minimumBlock = UINT32_MAX;
  GfxRenderer::Orientation previousOrientation = GfxRenderer::Portrait;
  GfxRenderer::RenderMode previousRenderMode = GfxRenderer::BW;
  bool previousInverted = false;
  RsvpReaderLayout layout;
  rsvp::Context context;
  rsvp::ContextPlacement contextPlacement;
  int contextSpine = -1;
  uint32_t contextOffset = UINT32_MAX;
  bool contextFrame = false;
  int64_t contextBuildUs = 0;
  uint32_t entryHeap = 0;
  uint32_t fontPresentations[rsvp::FONT_TIER_COUNT]{};
  uint32_t oversizedWords = 0;
  bool drawFrame(const RsvpToken* token, bool paused);
  void loadSome();
  void failSource();
  void sampleHeap();
  void reportSummary();
  void finishAtCurrentPosition();
};
