#pragma once

#include <cstddef>
#include <cstdint>

struct RsvpToken {
  static constexpr size_t MAX_TEXT_BYTES = 200;
  int spineIndex = 0;
  uint32_t startOffset = 0;
  uint32_t endOffset = 0;  // Exclusive visible-codepoint offset, including truncated text.
  uint16_t length = 0;
  char text[MAX_TEXT_BYTES + 1]{};
};

// Fixed recent history, including forward replay after a manual backward step.
class RsvpHistory {
 public:
  static constexpr size_t CAPACITY = 32;
  const RsvpToken* current() const { return count ? &tokens[(first + cursor) % CAPACITY] : nullptr; }
  const RsvpToken* peekPrevious() const {
    return count && cursor ? &tokens[(first + cursor - 1) % CAPACITY] : nullptr;
  }
  const RsvpToken* peekNext() const {
    return count && cursor + 1 < count ? &tokens[(first + cursor + 1) % CAPACITY] : nullptr;
  }
  bool previous() {
    if (!count || !cursor) return false;
    --cursor;
    return true;
  }
  bool replayNext() {
    if (!count || cursor + 1 >= count) return false;
    ++cursor;
    return true;
  }
  void append(const RsvpToken& token) {
    if (count == CAPACITY) {
      first = (first + 1) % CAPACITY;
      --count;
    }
    tokens[(first + count) % CAPACITY] = token;
    cursor = count++;
  }

 private:
  RsvpToken tokens[CAPACITY]{};
  size_t first = 0;
  size_t count = 0;
  size_t cursor = 0;
};
