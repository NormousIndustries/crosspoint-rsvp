#pragma once

#include <cstring>

#include "RsvpToken.h"

class RsvpTokenizer {
 public:
  using Emit = bool (*)(void*, const RsvpToken&);
  RsvpTokenizer(void* context, Emit emit) : context(context), emit(emit) {}
  void reset(int spineIndex) {
    token = RsvpToken{};
    token.spineIndex = spineIndex;
    codepointLength = expectedLength = 0;
    truncated = false;
    truncatedTokens = 0;
    pendingAvailable = false;
    scanner = {};
  }
  // Offsets come from the chapter parser, including whitespace and hidden-layout text.
  bool consume(const char* text, size_t size, uint32_t offset) {
    for (size_t i = 0; i < size; ++i) {
      const auto byte = static_cast<uint8_t>(text[i]);
      if (!codepointLength) {
        codepointOffset = offset++;
        expectedLength = byte < 0x80 ? 1 : byte < 0xE0 ? 2 : byte < 0xF0 ? 3 : 4;
      }
      codepoint[codepointLength++] = text[i];
      if (codepointLength == expectedLength) {
        uint32_t cp = static_cast<uint8_t>(codepoint[0]) & (expectedLength == 1 ? 0x7F : (0x7F >> expectedLength));
        for (uint8_t n = 1; n < expectedLength; ++n) cp = (cp << 6) | (static_cast<uint8_t>(codepoint[n]) & 0x3F);
        if (isSpace(cp) || cp == 0x200B) {
          if (!finishWord()) return false;
          if (cp == 0x2029 && !boundary(true)) return false;
        } else {
          if (!token.length && !emitPending()) return false;
          if (!token.length) token.startOffset = codepointOffset;
          token.endOffset = codepointOffset + 1;
          if (!truncated && token.length + expectedLength <= RsvpToken::MAX_TEXT_BYTES) {
            memcpy(token.text + token.length, codepoint, expectedLength);
            token.length += expectedLength;
          } else {
            truncated = true;
          }
          scanner.consume(cp, token.length, expectedLength);
        }
        codepointLength = 0;
      }
    }
    return true;
  }
  bool boundary(bool paragraph = false, bool chapter = false) {
    if (!finishWord()) return false;
    if (pendingAvailable) {
      pending.paragraphEnd = paragraph;
      pending.chapterEnd = chapter;
      if (paragraph || chapter) pending.timing = rsvp::maxTiming(pending.timing, rsvp::Timing::Paragraph);
    }
    return emitPending();
  }
  bool wordBoundary() { return finishWord(); }
  uint32_t truncations() const { return truncatedTokens; }

 private:
  bool emitPending() {
    if (!pendingAvailable) return true;
    if (!emit(context, pending)) return false;
    pendingAvailable = false;
    return true;
  }
  bool finishWord() {
    if (!token.length) return true;
    if (!emitPending()) return false;
    token.text[token.length] = '\0';
    token.timing = scanner.finish(token.text, truncated);
    token.paragraphEnd = token.chapterEnd = false;
    pending = token;
    pendingAvailable = true;
    if (truncated) ++truncatedTokens;
    token.length = 0;
    truncated = false;
    scanner = {};
    return true;
  }
  static bool isSpace(uint32_t cp) {
    return (cp >= 9 && cp <= 13) || cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F ||
           cp == 0x3000;
  }
  void* context;
  Emit emit;
  RsvpToken token;
  RsvpToken pending;
  rsvp::TimingScanner scanner;
  bool pendingAvailable = false;
  char codepoint[4]{};
  uint8_t codepointLength = 0;
  uint8_t expectedLength = 0;
  uint32_t codepointOffset = 0;
  bool truncated = false;
  uint32_t truncatedTokens = 0;
};
