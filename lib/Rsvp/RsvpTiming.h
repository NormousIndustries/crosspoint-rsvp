#pragma once

#include <Utf8.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rsvp {
enum class Timing : uint8_t { Normal, Long, Comma, Clause, Sentence, Paragraph, Count };
static constexpr uint16_t TIMING_MS[] = {0, 100, 150, 225, 350, 500};
static constexpr uint16_t LONG_WORD_LENGTH = 10;
constexpr uint16_t adjustmentMs(Timing timing) { return TIMING_MS[static_cast<uint8_t>(timing)]; }
constexpr Timing maxTiming(Timing a, Timing b) { return adjustmentMs(a) >= adjustmentMs(b) ? a : b; }

// ORP lexical ranges, excluding CJK punctuation; independent of font width.
inline bool timingLexical(uint32_t cp) {
  const bool cjkPunctuation = (cp >= 0x3000 && cp <= 0x303F) || (cp >= 0xFE10 && cp <= 0xFE1F) ||
      (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF01 && cp <= 0xFF0F) ||
      (cp >= 0xFF1A && cp <= 0xFF20) || (cp >= 0xFF3B && cp <= 0xFF40) || (cp >= 0xFF5B && cp <= 0xFF65);
  return (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
         (cp >= 0xC0 && cp <= 0x2AF && cp != 0xD7 && cp != 0xF7) || (cp >= 0x1E00 && cp <= 0x1EFF) ||
         (cp >= 0x370 && cp <= 0x52F) || (utf8IsCjkCodepoint(cp) && !cjkPunctuation);
}
inline bool timingWrapper(uint32_t cp) {
  return cp == '"' || cp == '\'' || cp == '(' || cp == ')' || cp == '[' || cp == ']' || cp == '{' || cp == '}' ||
         cp == 0x2018 || cp == 0x2019 || cp == 0x201C || cp == 0x201D || cp == 0xAB || cp == 0xBB ||
         cp == 0xFF08 || cp == 0xFF09 || (cp >= 0x3008 && cp <= 0x3011);
}
inline bool abbreviation(std::string_view text) {
  // Trim ASCII wrappers; Unicode wrappers are handled by the scanner's byte extent.
  while (!text.empty() && timingWrapper(static_cast<uint8_t>(text.front()))) text.remove_prefix(1);
  static constexpr std::string_view ABBREVIATIONS[] = {
      "mr.", "mrs.", "ms.", "dr.", "prof.", "sr.", "jr.", "st.", "vs.", "etc.", "e.g.", "i.e.",
      "fig.", "no.", "inc.", "ltd.", "a.m.", "p.m."};
  for (const auto candidate : ABBREVIATIONS) {
    if (candidate.size() != text.size()) continue;
    bool equal = true;
    for (size_t i = 0; i < text.size(); ++i) {
      const char c = text[i] >= 'A' && text[i] <= 'Z' ? text[i] + ('a' - 'A') : text[i];
      if (c != candidate[i]) { equal = false; break; }
    }
    if (equal) return true;
  }
  // A single initial or dotted initials: A., U.S., Ph.D. is deliberately not inferred.
  if (text.empty() || text.size() % 2) return false;
  for (size_t i = 0; i < text.size(); i += 2) {
    if (!((text[i] >= 'A' && text[i] <= 'Z') || (text[i] >= 'a' && text[i] <= 'z')) || text[i + 1] != '.')
      return false;
  }
  return true;
}

class TimingScanner {
 public:
  void consume(uint32_t cp, uint16_t storedEnd, uint8_t bytes) {
    if (timingLexical(cp) && lexicalLength < UINT16_MAX) ++lexicalLength;
    if (timingWrapper(cp)) return;
    if (!terminal) significantStart = storedEnd >= bytes ? storedEnd - bytes : 0;
    dotRun = cp == '.' ? dotRun + 1 : 0;
    terminal = cp;
    significantEnd = storedEnd;
  }
  Timing finish(const char* text, bool truncated) const {
    Timing result = lexicalLength >= LONG_WORD_LENGTH ? Timing::Long : Timing::Normal;
    if (terminal == ',' || terminal == 0xFF0C || terminal == 0x3001) return maxTiming(result, Timing::Comma);
    if (terminal == ':' || terminal == ';' || terminal == 0xFF1A || terminal == 0xFF1B)
      return maxTiming(result, Timing::Clause);
    if (terminal == '!' || terminal == '?' || terminal == 0x2026 || terminal == 0x3002 ||
        terminal == 0xFF01 || terminal == 0xFF1F || terminal == 0xFF0E) return maxTiming(result, Timing::Sentence);
    if (terminal == '.' && (truncated || dotRun > 1 ||
                           !abbreviation({text + significantStart, static_cast<size_t>(significantEnd - significantStart)})))
      return maxTiming(result, Timing::Sentence);
    return result;
  }

 private:
  uint16_t lexicalLength = 0;
  uint16_t significantEnd = 0;
  uint16_t significantStart = 0;
  uint32_t terminal = 0;
  uint32_t dotRun = 0;
};
}  // namespace rsvp
