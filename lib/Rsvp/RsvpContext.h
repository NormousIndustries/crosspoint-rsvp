#pragma once

#include <algorithm>
#include <cstring>

#include "RsvpToken.h"

namespace rsvp {
// Ephemeral SD records, never an EPUB layout-cache format.
static constexpr uint32_t RECORD_HEADER_BYTES = 16;
inline bool sentenceEnd(const RsvpToken& token) {
  return token.paragraphEnd || token.chapterEnd || token.timing == Timing::Sentence ||
         token.timing == Timing::Paragraph;
}

struct Context {
  static constexpr size_t TEXT_BYTES = 2048;
  static constexpr uint8_t MAX_WORDS = 49;
  static constexpr uint8_t NEIGHBOURS = 24;
  char text[TEXT_BYTES]{};
  uint16_t offsets[MAX_WORDS]{};
  uint8_t count = 0;
  uint8_t selected = 0;
  uint16_t used = 0;
  bool leading = false;
  bool trailing = false;
  bool append(const RsvpToken& token) {
    if (!token.length || token.length > RsvpToken::MAX_TEXT_BYTES || count == MAX_WORDS ||
        static_cast<size_t>(used) + token.length + 1 > TEXT_BYTES)
      return false;
    offsets[count++] = used;
    memcpy(text + used, token.text, token.length);
    used += token.length;
    text[used++] = '\0';
    return true;
  }
  void fallback(const RsvpToken& token) {
    count = selected = used = 0;
    leading = trailing = false;
    append(token);
  }
};

using ReadContextToken = bool (*)(void*, uint32_t, RsvpToken&);
// Separate read cursor: this function cannot consume the playback stream.
inline bool extractContext(Context& out, const RsvpToken& selected, void* ctx, ReadContextToken read, uint32_t fileSize,
                           RsvpToken& scratch) {
  out.fallback(selected);
  if (!read(ctx, selected.fileOffset, scratch) || scratch.startOffset != selected.startOffset ||
      scratch.endOffset != selected.endOffset || scratch.spineIndex != selected.spineIndex)
    return false;
  uint32_t first = selected.fileOffset;
  uint32_t previous = scratch.previousFileOffset;
  size_t beforeBytes = 0;
  bool leading = false;
  for (uint8_t n = 0; previous != UINT32_MAX; ++n) {
    if (previous >= first || !read(ctx, previous, scratch)) return false;
    if (sentenceEnd(scratch)) break;
    if (n == Context::NEIGHBOURS || beforeBytes + scratch.length + 1 > Context::TEXT_BYTES / 2) {
      leading = true;
      break;
    }
    beforeBytes += scratch.length + 1;
    first = previous;
    previous = scratch.previousFileOffset;
  }
  out.count = out.selected = out.used = 0;
  out.leading = leading;
  out.trailing = false;
  uint32_t position = first;
  bool found = false;
  uint8_t after = 0;
  while (position < fileSize) {
    if (!read(ctx, position, scratch)) {
      out.fallback(selected);
      return false;
    }
    if (!out.append(scratch)) {
      out.trailing = true;
      break;
    }
    if (position == selected.fileOffset) {
      out.selected = out.count - 1;
      found = true;
    }
    if (sentenceEnd(scratch)) break;
    position += RECORD_HEADER_BYTES + scratch.length;
    if (found && ++after > Context::NEIGHBOURS) {
      out.trailing = position < fileSize;
      break;
    }
  }
  if (!found) {
    out.fallback(selected);
    return false;
  }
  return true;
}

// Remove a complete extended cluster for excerpting an over-wide token. Never
// leave a base without its marks, a dangling ZWJ, or half a regional-indicator pair.
inline size_t previousCluster(const char* text, size_t size) {
  size_t cluster = 0;
  uint32_t previous = 0;
  unsigned regional = 0;
  for (size_t i = 0; i < size;) {
    const size_t start = i;
    const auto byte = static_cast<uint8_t>(text[i++]);
    const unsigned bytes = byte < 0x80 ? 1 : byte < 0xE0 ? 2 : byte < 0xF0 ? 3 : 4;
    uint32_t cp = byte & (bytes == 1 ? 0x7F : 0x7F >> bytes);
    for (unsigned n = 1; n < bytes && i < size; ++n) cp = (cp << 6) | (static_cast<uint8_t>(text[i++]) & 0x3F);
    const bool ri = cp >= 0x1F1E6 && cp <= 0x1F1FF;
    const bool continuation = utf8IsCombiningMark(cp) || cp == 0x200D || previous == 0x200D ||
                              (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xE0100 && cp <= 0xE01EF) ||
                              (cp >= 0x1F3FB && cp <= 0x1F3FF) || (ri && regional % 2);
    if (!continuation) cluster = start;
    regional = ri ? regional + 1 : 0;
    previous = cp;
  }
  return cluster;
}

struct ContextPlacement {
  uint8_t first = 0;
  uint8_t last = 0;  // Exclusive.
  uint8_t lines = 0;
  bool leading = false;
  bool trailing = false;
  uint16_t selectedBytes = 0;
  bool shortened = false;
  int16_t x[Context::MAX_WORDS]{};
  uint8_t row[Context::MAX_WORDS]{};
  int16_t leadingX = 0;
  int16_t trailingX = 0;
  uint8_t trailingRow = 0;
  char scratch[RsvpToken::MAX_TEXT_BYTES + 1]{};
};
using MeasureContext = int (*)(void*, const char*);

inline bool packContext(const Context& text, ContextPlacement& p, int width, int rows, int gap, const char* ellipsis,
                        void* ctx, MeasureContext measure) {
  const int dots = measure(ctx, ellipsis);
  int x = 0;
  uint8_t row = 0;
  if (p.leading) {
    p.leadingX = 0;
    x = dots + gap;
  }
  for (uint8_t i = p.first; i < p.last; ++i) {
    const char* word = text.text + text.offsets[i];
    if (i == text.selected && p.shortened) {
      memcpy(p.scratch, word, p.selectedBytes);
      p.scratch[p.selectedBytes] = '\0';
      word = p.scratch;
    }
    const int w = measure(ctx, word) + (i == text.selected && p.shortened ? gap + dots : 0);
    if (w > width) return false;
    if (x && x + w > width) {
      x = 0;
      ++row;
    }
    if (row >= rows) return false;
    p.x[i] = x;
    p.row[i] = row;
    x += w + gap;
  }
  if (p.trailing) {
    if (x + dots > width) {
      x = 0;
      ++row;
    }
    if (row >= rows) return false;
    p.trailingX = x;
    p.trailingRow = row;
  }
  p.lines = row + 1;
  return true;
}

inline bool fitContext(const Context& text, ContextPlacement& p, int width, int rows, int gap, const char* ellipsis,
                       void* ctx, MeasureContext measure) {
  p = {};
  if (!text.count || width <= 0 || rows <= 0 || text.selected >= text.count) return false;
  p.last = text.count;
  p.leading = text.leading;
  p.trailing = text.trailing;
  p.selectedBytes = strlen(text.text + text.offsets[text.selected]);
  while (!packContext(text, p, width, rows, gap, ellipsis, ctx, measure)) {
    const int before = text.selected - p.first;
    const int after = p.last - text.selected - 1;
    if (before || after) {
      if (before >= after && before) {
        ++p.first;
        p.leading = true;
      } else {
        --p.last;
        p.trailing = true;
      }
    } else {
      if (!p.selectedBytes) return false;
      p.shortened = true;
      p.selectedBytes = previousCluster(text.text + text.offsets[text.selected], p.selectedBytes);
    }
  }
  return true;
}
}  // namespace rsvp
