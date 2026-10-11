#include "RsvpTypography.h"

#include <Utf8.h>

#include <algorithm>

#include "fonts/rsvp_132.h"
#include "fonts/rsvp_64.h"
#include "fonts/rsvp_96.h"

namespace rsvp {
namespace {
bool lexical(uint32_t cp) {
  return (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
         (cp >= 0xC0 && cp <= 0x2AF && cp != 0xD7 && cp != 0xF7) || (cp >= 0x1E00 && cp <= 0x1EFF) ||
         (cp >= 0x370 && cp <= 0x52F) || utf8IsCjkCodepoint(cp);
}
}  // namespace

Orp findOrp(const char* text) {
  Orp result;
  if (!text) return result;
  const auto* cursor = reinterpret_cast<const uint8_t*>(text);
  uint32_t cp;
  while ((cp = utf8NextCodepoint(&cursor))) {
    if (lexical(cp)) ++result.lexicalLength;
  }
  result.lexicalIndex = orpIndex(result.lexicalLength);
  cursor = reinterpret_cast<const uint8_t*>(text);
  uint16_t index = 0;
  while (*cursor) {
    const auto* start = cursor;
    cp = utf8NextCodepoint(&cursor);
    if (lexical(cp) && index++ == result.lexicalIndex) {
      result.byteOffset = start - reinterpret_cast<const uint8_t*>(text);
      break;
    }
  }
  return result;
}

EpdFont font(FontSize size) {
  switch (size) {
    case FontSize::Normal:
      return EpdFont(&rsvp132);
    case FontSize::Long:
      return EpdFont(&rsvp96);
    case FontSize::ExtraLong:
      return EpdFont(&rsvp64);
  }
  return EpdFont(&rsvp64);
}

WordMetrics walk(const EpdFont& face, const char* text, size_t orpByte, void* ctx, DrawGlyph draw) {
  WordMetrics result;
  if (!text) return result;
  const auto* cursor = reinterpret_cast<const uint8_t*>(text);
  int baseX = 0;
  int baseLeft = 0;
  int baseWidth = 0;
  int baseTop = 0;
  uint32_t previous = 0;
  int32_t advance = 0;
  bool haveInk = false;
  while (*cursor) {
    const size_t byte = cursor - reinterpret_cast<const uint8_t*>(text);
    const uint32_t cp = utf8NextCodepoint(&cursor);
    const EpdGlyph* glyph = face.getGlyph(cp);
    if (!glyph) continue;
    const bool mark = utf8IsCombiningMark(cp);
    const auto anchor = combiningMark::anchorFor(cp);
    if (!mark && previous) baseX += fp4::toPixel(advance + face.getKerning(previous, cp));
    const int x = mark ? combiningMark::anchorOver(anchor, baseX, baseLeft, baseWidth, glyph->left, glyph->width)
                       : baseX;
    const int y = mark ? -combiningMark::raiseAboveBase(anchor, glyph->top, glyph->height, baseTop) : 0;
    if (glyph->width && glyph->height) {
      const int left = x + glyph->left;
      const int top = y - glyph->top;
      result.left = haveInk ? std::min(result.left, left) : left;
      result.right = haveInk ? std::max(result.right, left + glyph->width) : left + glyph->width;
      result.top = haveInk ? std::min(result.top, top) : top;
      result.bottom = haveInk ? std::max(result.bottom, top + glyph->height) : top + glyph->height;
      haveInk = true;
      if (draw) draw(ctx, *glyph, face.data->bitmap + glyph->dataOffset, x, y);
    }
    if (byte == orpByte) {
      result.orpCenter2 = glyph->width ? 2 * (x + glyph->left) + glyph->width : 2 * x + fp4::toPixel(glyph->advanceX);
    }
    if (!mark) {
      baseLeft = glyph->left;
      baseWidth = glyph->width;
      baseTop = glyph->top;
      previous = cp;
      advance = glyph->advanceX;
    }
  }
  return result;
}

Placement place(const char* text, const Orp& orp, int focusX, int left, int right, int top, int bottom) {
  Placement result;
  static constexpr FontSize SIZES[] = {FontSize::Normal, FontSize::Long, FontSize::ExtraLong};
  for (const auto size : SIZES) {
    result.size = size;
    result.metrics = walk(font(size), text, orp.byteOffset);
    result.x = focusX - (result.metrics.orpCenter2 + 1) / 2;
    result.fits = result.x + result.metrics.left >= left && result.x + result.metrics.right <= right &&
                  result.metrics.top >= top && result.metrics.bottom <= bottom;
    if (result.fits) break;
  }
  return result;
}
}  // namespace rsvp
