#pragma once

#include <atomic>

#include "activities/Activity.h"
#include "components/themes/BaseTheme.h"

// Temporary X3 panel-throughput experiment; independent of ReaderActivity.
class RsvpDisplayTestActivity final : public Activity {
 public:
  RsvpDisplayTestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("RsvpDisplayTest", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool preventAutoSleep() override { return true; }
  bool handleForcedRefresh() override { return true; }

 private:
  static constexpr uint16_t WORD_COUNT = 100;
  static constexpr uint16_t SPEEDS[] = {300, 375, 500};
  enum Control : uint8_t { PLAY = 1, PAUSE = 2, PREVIOUS = 4, NEXT = 8, FASTER = 16, SLOWER = 32, EXIT = 64 };
  enum class DisplayJob : uint8_t { None, Chrome, Timed };
  struct Sample {
    int64_t deadline;
    int64_t renderStart;
    int64_t rasterDone;
    int64_t displayStart;
    int64_t displayReturn;
    uint16_t word;
    HalDisplay::DisplayMeasurement panel;
  };

  GfxRenderer::Orientation previousOrientation = GfxRenderer::Portrait;
  GfxRenderer::RenderMode previousRenderMode = GfxRenderer::BW;
  Rect wordRect;
  Rect safeRect;
  uint16_t wordOffsets[WORD_COUNT]{};
  uint16_t displayedWord = 0;
  uint16_t targetWord = 0;
  char wordBuffer[32]{};
  std::atomic<bool> playing{false};
  // Only playing crosses tasks without RenderLock; controls are loop-task-only.
  uint8_t pendingControls = 0;
  uint8_t speedIndex = 0;
  DisplayJob displayJob = DisplayJob::Chrome;
  bool reportPending = false;
  bool runNeedsBaseline = false;
  bool initialFrame = true;
  int64_t nextDeadline = 0;
  int64_t committedDeadline = 0;
  int64_t baselineStart = 0;
  int64_t baselineReturn = 0;
  uint16_t runWpm = 300;
  uint16_t sampleCount = 0;
  // Inline storage: one fallible activity allocation, no per-transition heap work.
  Sample samples[WORD_COUNT - 1]{};
  HalDisplay::DisplayMeasurement panelMeasurement;
  uint32_t minimumFreeHeap = UINT32_MAX;
  uint32_t minimumLargestBlock = UINT32_MAX;
  uint32_t lastFreeHeap = 0;
  uint32_t lastLargestBlock = 0;

  bool loadPassage();
  void drawWord(uint16_t index);
  void drawChrome();
  void sampleHeap();
  void printReport(const char* reason);
};
