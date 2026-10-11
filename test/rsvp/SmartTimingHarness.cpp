#include "RsvpCadence.h"
#include "RsvpTokenizer.h"

#ifdef RSVP_FREESTANDING
extern "C" void* memset(void* dest, int value, size_t size) {
  auto* p = static_cast<unsigned char*>(dest);
  for (size_t i = 0; i < size; ++i) p[i] = value;
  return dest;
}
extern "C" void* memcpy(void* dest, const void* source, size_t size) {
  auto* d = static_cast<unsigned char*>(dest);
  const auto* s = static_cast<const unsigned char*>(source);
  for (size_t i = 0; i < size; ++i) d[i] = s[i];
  return dest;
}
extern "C" int memcmp(const void* a, const void* b, size_t size) {
  const auto* x = static_cast<const unsigned char*>(a);
  const auto* y = static_cast<const unsigned char*>(b);
  for (size_t i = 0; i < size; ++i) if (x[i] != y[i]) return x[i] - y[i];
  return 0;
}
extern "C" size_t strlen(const char* s) {
  size_t size = 0;
  while (s[size]) ++size;
  return size;
}
extern "C" int strcmp(const char* a, const char* b) {
  while (*a && *a == *b) { ++a; ++b; }
  return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}
#endif

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
namespace {
struct Capture {
  RsvpToken tokens[64]{};
  size_t count = 0;
  bool fail = false;
  RsvpTokenizer tokenizer{this, [](void* context, const RsvpToken& token) {
    auto& self = *static_cast<Capture*>(context);
    if (self.fail || self.count == 64) return false;
    self.tokens[self.count++] = token;
    return true;
  }};
  Capture() { tokenizer.reset(0); }
  bool text(const char* text, uint32_t offset = 0) { return tokenizer.consume(text, strlen(text), offset); }
  void reset(int chapter = 0) { count = 0; fail = false; tokenizer.reset(chapter); }
};
int classification() {
  Capture capture;
  struct Case { const char* text; rsvp::Timing timing; };
  static constexpr Case CASES[] = {
    {"word", rsvp::Timing::Normal}, {"tomorrow", rsvp::Timing::Normal},
    {"extraordinary", rsvp::Timing::Long}, {"abcdefghij", rsvp::Timing::Long},
    {"abcdefghi", rsvp::Timing::Normal}, {"however,", rsvp::Timing::Comma},
    {"extraordinary,", rsvp::Timing::Comma}, {"however:", rsvp::Timing::Clause},
    {"however;", rsvp::Timing::Clause}, {"tomorrow.", rsvp::Timing::Sentence},
    {"extraordinary.", rsvp::Timing::Sentence}, {"why?", rsvp::Timing::Sentence},
    {"Go!", rsvp::Timing::Sentence}, {"\"tomorrow.\"", rsvp::Timing::Sentence},
    {"(tomorrow.)", rsvp::Timing::Sentence}, {"[tomorrow!]", rsvp::Timing::Sentence},
    {"\xE2\x80\x9Ctomorrow.\xE2\x80\x9D", rsvp::Timing::Sentence},
    {"don't", rsvp::Timing::Normal}, {"don\xE2\x80\x99t", rsvp::Timing::Normal},
    {"3.14", rsvp::Timing::Normal}, {"3.14.", rsvp::Timing::Sentence},
    {"Dr.", rsvp::Timing::Normal}, {"MR.", rsvp::Timing::Normal},
    {"\xE2\x80\x9C" "Dr.\xE2\x80\x9D", rsvp::Timing::Normal},
    {"A.", rsvp::Timing::Normal}, {"U.S.", rsvp::Timing::Normal},
    {"e.g.", rsvp::Timing::Normal}, {"Wait...", rsvp::Timing::Sentence},
    {"Dr...", rsvp::Timing::Sentence}, {"Wait\xE2\x80\xA6", rsvp::Timing::Sentence},
    {"well-known", rsvp::Timing::Normal}, {"state-of-the-art", rsvp::Timing::Long},
    {"\"abcdefghij\"", rsvp::Timing::Long}, {"!!!", rsvp::Timing::Sentence},
    {"(abcdefghi)", rsvp::Timing::Normal}, {"123,456.78", rsvp::Timing::Normal},
    {"abcdefghi\xEF\xBC\x8C", rsvp::Timing::Comma},
    {"abcdefghi\xEF\xBC\x9B", rsvp::Timing::Clause},
    {"word\xE3\x80\x82", rsvp::Timing::Sentence},
    {"abcdefghi\xE3\x80\x8D", rsvp::Timing::Normal}
  };
  for (const auto& item : CASES) {
    capture.reset();
    CHECK(capture.text(item.text));
    CHECK(capture.tokenizer.boundary());
    CHECK(capture.count == 1);
    CHECK(capture.tokens[0].timing == item.timing);
    CHECK(strcmp(capture.tokens[0].text, item.text) == 0);
    CHECK(!capture.tokens[0].paragraphEnd && !capture.tokens[0].chapterEnd);
  }
  return 0;
}
int structure() {
  Capture c;
  // <p>Hello <em>extraordinary.</em> \n</p><p>don't<br/>stop</p>
  CHECK(c.text("Hello "));
  CHECK(c.text("extra", 6));
  CHECK(c.text("ordinary. \n", 11));
  CHECK(c.tokenizer.boundary(true));
  CHECK(c.count == 2 && c.tokens[0].timing == rsvp::Timing::Normal);
  CHECK(c.tokens[1].timing == rsvp::Timing::Paragraph && c.tokens[1].paragraphEnd);
  CHECK(c.tokens[1].startOffset == 6 && c.tokens[1].endOffset == 20);
  CHECK(c.tokenizer.boundary(true));  // Empty/open block does not create a token.
  CHECK(c.text("don't", 22));
  CHECK(c.tokenizer.wordBoundary());
  CHECK(c.text("stop \n", 27));
  CHECK(c.tokenizer.boundary(true));
  CHECK(c.count == 4 && !c.tokens[2].paragraphEnd && c.tokens[3].paragraphEnd);
  CHECK(c.tokenizer.boundary(false, true));
  CHECK(c.count == 4);
  c.reset(1);
  CHECK(c.text("Next chapter", 0));
  CHECK(c.tokenizer.boundary(false, true));
  CHECK(c.count == 2 && c.tokens[1].chapterEnd && c.tokens[1].timing == rsvp::Timing::Paragraph);
  CHECK(c.tokens[1].spineIndex == 1);
  c.reset();
  CHECK(c.text("one\xE2\x80\xA9two"));
  CHECK(c.tokenizer.boundary());
  CHECK(c.count == 2 && c.tokens[0].paragraphEnd && !c.tokens[1].paragraphEnd);
  c.reset();
  CHECK(c.text("\xC3"));
  CHECK(c.text("\xA9lan\xC2\xA0word", 1));
  CHECK(c.tokenizer.boundary());
  CHECK(c.count == 2 && c.tokens[0].endOffset == 4 && c.tokens[1].startOffset == 5);
  c.reset();
  for (int i = 0; i < 250; ++i) CHECK(c.text("a", i));
  CHECK(c.text(". ", 250));
  CHECK(c.tokenizer.boundary());
  CHECK(c.count == 1 && c.tokens[0].length == 200 && c.tokens[0].endOffset == 251);
  CHECK(c.tokens[0].timing == rsvp::Timing::Sentence && c.tokenizer.truncations() == 1);
  c.reset();
  c.fail = true;
  CHECK(c.text("word "));
  CHECK(!c.tokenizer.boundary());
  return 0;
}
int scheduling() {
  static constexpr uint16_t SPEEDS[] = {50, 75, 100, 125};
  static constexpr uint16_t EXTRAS[] = {0, 100, 150, 225, 350, 500};
  for (size_t speed = 0; speed < 4; ++speed) {
    RsvpCadence cadence;
    CHECK(cadence.wpm() == 100);
    for (int i = 0; i < 5; ++i) cadence.slower();
    CHECK(cadence.wpm() == 50);
    for (size_t i = 0; i < speed; ++i) cadence.faster();
    CHECK(cadence.wpm() == SPEEDS[speed]);
    for (uint8_t reason = 0; reason < 6; ++reason) {
      const int64_t target = (60000 / SPEEDS[speed] + EXTRAS[reason]) * 1000LL;
      cadence.resetSummary();
      cadence.completed(1000000, 1480000, static_cast<rsvp::Timing>(reason));
      CHECK(!cadence.due(1000000 + target - 1));
      CHECK(cadence.due(1000000 + target));
      CHECK(cadence.wordsPresented == 1 && cadence.deadlineMisses == 0);
      CHECK(cadence.timingCounts[reason] == 1 && cadence.additionalUs == EXTRAS[reason] * 1000LL);
      cadence.completed(1000000 + target, 1480000 + target, static_cast<rsvp::Timing>(reason));
      CHECK(cadence.effectiveWpm() == 60000000 / target);
      cadence.rebase(9000000);
      CHECK(!cadence.due(9000000 + target - 1));
      CHECK(cadence.due(9000000 + target));
    }
  }
  RsvpCadence c;
  for (int i = 0; i < 10; ++i) c.faster();
  CHECK(c.wpm() == 125);
  c.completed(1000000, 1900000);
  CHECK(c.deadlineMisses == 1 && c.due(1900000));
  c.completed(1901000, 2381000);
  CHECK(c.wordsPresented == 2 && !c.due(2380999));
  c.resetSummary();
  CHECK(c.additionalUs == 0 && c.wordsPresented == 0);
  c.slower();
  c.completed(1000000, 1480000);
  c.suspend(1600000);
  c.resume(4600000);
  CHECK(c.elapsedUs() == 0);  // Also valid if loading ends in an overflow/source pause.
  c.completed(4600000, 5080000);
  CHECK(c.effectiveWpm() == 100);  // Three seconds chapter I/O excluded.
  return 0;
}
int navigation() {
  RsvpHistory history;
  RsvpToken token;
  token.startOffset = 42;
  token.timing = rsvp::Timing::Sentence;
  history.append(token);
  RsvpCadence c;
  c.completed(1000000, 1480000, history.current()->timing);
  CHECK(history.current()->startOffset == 42);
  // Pause does not mutate history. Resume intentionally presents Current again.
  c.resetSummary();
  c.completed(5000000, 5480000, history.current()->timing);
  CHECK(!c.due(5949999) && c.due(5950000));
  CHECK(history.current()->startOffset == 42 && c.wordsPresented == 1);
  token.spineIndex = 1;
  token.startOffset = 0;
  token.timing = rsvp::Timing::Paragraph;
  history.append(token);
  CHECK(history.previous() && history.current()->startOffset == 42);
  CHECK(history.replayNext() && history.current()->spineIndex == 1);
  CHECK(history.current()->timing == rsvp::Timing::Paragraph);
  for (uint32_t i = 0; i < 1000; ++i) { token.startOffset = i; history.append(token); }
  for (size_t i = 1; i < RsvpHistory::CAPACITY; ++i) CHECK(history.previous());
  CHECK(!history.previous() && history.current()->startOffset == 968);
  return 0;
}
}  // namespace
extern "C" int testMain() {
  if (const int failure = classification()) return failure;
  if (const int failure = structure()) return failure;
  if (const int failure = scheduling()) return failure;
  return navigation();
}
#ifndef RSVP_FREESTANDING
int main() { return testMain(); }
#endif
