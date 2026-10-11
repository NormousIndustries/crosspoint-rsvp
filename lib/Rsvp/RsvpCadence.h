#pragma once

#include <cstdint>

#include "RsvpTiming.h"

class RsvpCadence {
 public:
  static constexpr uint16_t SPEEDS[] = {50, 75, 100, 125};
  uint16_t wpm() const { return SPEEDS[speedIndex]; }
  int64_t intervalUs() const { return (60000 / wpm()) * 1000LL; }
  void faster() {
    if (speedIndex + 1u < sizeof(SPEEDS) / sizeof(SPEEDS[0])) ++speedIndex;
  }
  void slower() {
    if (speedIndex) --speedIndex;
  }
  void rebase(int64_t now) { nextStart = now + intervalUs() + rsvp::adjustmentMs(currentTiming) * 1000LL; }
  bool due(int64_t now) const { return now >= nextStart; }
  // One presentation per submission; an overrun makes exactly one next word due.
  void completed(int64_t start, int64_t end, rsvp::Timing timing = rsvp::Timing::Normal) {
    currentTiming = timing;
    const int64_t additional = rsvp::adjustmentMs(timing) * 1000LL;
    nextStart = start + intervalUs() + additional;
    if (end > nextStart) ++deadlineMisses;
    if (!wordsPresented) firstStart = start;
    lastStart = start;
    ++timingCounts[static_cast<uint8_t>(timing)];
    additionalUs += additional;
    ++wordsPresented;
    serviceUs += end - start;
  }
  void resetSummary() {
    wordsPresented = deadlineMisses = 0;
    firstStart = lastStart = serviceUs = 0;
    additionalUs = 0;
    for (auto& count : timingCounts) count = 0;
  }
  int64_t elapsedUs() const { return lastStart - firstStart; }
  uint32_t effectiveWpm() const {
    return wordsPresented > 1 && elapsedUs() > 0 ? (wordsPresented - 1) * 60000000ULL / elapsedUs() : 0;
  }
  uint32_t wordsPresented = 0;
  uint32_t deadlineMisses = 0;
  int64_t serviceUs = 0;
  int64_t additionalUs = 0;
  uint32_t timingCounts[static_cast<uint8_t>(rsvp::Timing::Count)]{};

  // Exclude chapter loading from reading cadence without changing the next deadline.
  void suspend(int64_t now) { suspendedAt = now; }
  void resume(int64_t now) {
    if (wordsPresented) {
      firstStart += now - suspendedAt;
      lastStart += now - suspendedAt;
    }
  }

 private:
  uint8_t speedIndex = 2;
  int64_t nextStart = 0;
  int64_t firstStart = 0;
  int64_t lastStart = 0;
  int64_t suspendedAt = 0;
  rsvp::Timing currentTiming = rsvp::Timing::Normal;
};
