#pragma once

#include <EpdFont.h>

#include <cstddef>
#include <cstdint>

namespace rsvp {
enum class FontSize : uint8_t { Normal, Long, ExtraLong, Compact, Small };
inline constexpr uint8_t FONT_TIER_COUNT = 5;

struct Orp {
  size_t byteOffset = 0;
  uint16_t lexicalLength = 0;
  uint8_t lexicalIndex = 0;
};

struct WordMetrics {
  int left = 0;
  int right = 0;  // Exclusive ink bounds, relative to the text origin.
  int top = 0;
  int bottom = 0;
  int orpCenter2 = 0;  // Twice the ink center, preserving half pixels.
};

struct Placement {
  WordMetrics metrics;
  FontSize size = FontSize::Normal;
  int x = 0;
  bool fits = true;
};

using DrawGlyph = void (*)(void*, const EpdGlyph&, const uint8_t*, int, int);

constexpr uint8_t orpIndex(uint16_t length) {
  return length <= 1 ? 0 : length <= 5 ? 1 : length <= 9 ? 2 : length <= 13 ? 3 : 4;
}

Orp findOrp(const char* text);
EpdFont font(FontSize size);
// Measurement and drawing share the exact advance, kerning and mark placement.
WordMetrics walk(const EpdFont& font, const char* text, size_t orpByte, void* ctx = nullptr, DrawGlyph draw = nullptr);
Placement place(const char* text, const Orp& orp, int focusX, int left, int right, int top = -INT32_MAX,
                int bottom = INT32_MAX);
}  // namespace rsvp
