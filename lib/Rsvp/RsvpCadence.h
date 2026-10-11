#pragma once

#include <cstdint>

class RsvpCadence {
 public:
  static constexpr uint16_t SPEEDS[] = {50, 75, 100, 125};
  uint16_t wpm() const { return SPEEDS[speedIndex]; }
  int64_t intervalUs() const { return 60000000LL / wpm(); }
  void faster() {
    if (speedIndex + 1 < sizeof(SPEEDS) / sizeof(SPEEDS[0])) ++speedIndex;
  }
  void slower() {
    if (speedIndex) --speedIndex;
  }
  void rebase(int64_t now) { nextStart = now + intervalUs(); }
  bool due(int64_t now) const { return now >= nextStart; }
  // One presentation per submission; an overrun makes exactly one next word due.
  void completed(int64_t start, int64_t end) {
    nextStart = start + intervalUs();
    if (end > nextStart) ++deadlineMisses;
    if (!wordsPresented) firstStart = start;
    lastStart = start;
    ++wordsPresented;
    serviceUs += end - start;
  }
  void resetSummary() {
    wordsPresented = deadlineMisses = 0;
    firstStart = lastStart = serviceUs = 0;
  }
  int64_t elapsedUs() const { return lastStart - firstStart; }
  uint32_t effectiveWpm() const {
    return wordsPresented > 1 && elapsedUs() > 0 ? (wordsPresented - 1) * 60000000ULL / elapsedUs() : 0;
  }
  uint32_t wordsPresented = 0;
  uint32_t deadlineMisses = 0;
  int64_t serviceUs = 0;

 private:
  uint8_t speedIndex = 2;
  int64_t nextStart = 0;
  int64_t firstStart = 0;
  int64_t lastStart = 0;
};
