#include <EpdFont.h>
#include <builtinFonts/notosans_16_regular.h>

#include <new>

#include "RsvpCadence.h"
#include "RsvpContext.h"
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
extern "C" size_t strlen(const char* s) {
  size_t size = 0;
  while (s[size]) ++size;
  return size;
}
extern "C" int strcmp(const char* a, const char* b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}
#endif

#define CHECK(condition)               \
  do {                                 \
    if (!(condition)) return __LINE__; \
  } while (false)
namespace {
struct Capture {
  RsvpToken tokens[128]{};
  size_t count = 0;
  uint32_t bytes = 0;
  uint32_t reads = 0;
  uint32_t badRecord = UINT32_MAX;
  RsvpTokenizer tokenizer{this, [](void* ctx, const RsvpToken& token) {
                            auto& self = *static_cast<Capture*>(ctx);
                            if (self.count == 128) return false;
                            auto& out = self.tokens[self.count];
                            out = token;
                            out.fileOffset = self.bytes;
                            out.previousFileOffset = self.count ? self.tokens[self.count - 1].fileOffset : UINT32_MAX;
                            self.bytes += rsvp::RECORD_HEADER_BYTES + token.length;
                            ++self.count;
                            return true;
                          }};
  void reset(int chapter = 0) {
    count = bytes = reads = 0;
    badRecord = UINT32_MAX;
    tokenizer.reset(chapter);
  }
  bool text(const char* text, uint32_t offset = 0) { return tokenizer.consume(text, strlen(text), offset); }
  bool finish() { return tokenizer.boundary(false, true); }
  static bool read(void* ctx, uint32_t position, RsvpToken& token) {
    auto& self = *static_cast<Capture*>(ctx);
    ++self.reads;
    if (self.badRecord == position) return false;
    for (size_t i = 0; i < self.count; ++i)
      if (self.tokens[i].fileOffset == position) {
        token = self.tokens[i];
        return true;
      }
    return false;
  }
} capture;
rsvp::Context context;
rsvp::ContextPlacement placement;
RsvpToken scratch;
EpdFont font(&notosans_16_regular);
int measure(void*, const char* text) {
  int w, h;
  font.getTextDimensions(text, &w, &h);
  return w;
}
bool extract(size_t selected) {
  return rsvp::extractContext(context, capture.tokens[selected], &capture, Capture::read, capture.bytes, scratch);
}
bool fit(int width = 456, int rows = 7) {
  return rsvp::fitContext(context, placement, width, rows, fp4::toPixel(font.getGlyph(' ')->advanceX) + 2, "…", nullptr,
                          measure);
}

int extraction() {
  capture.reset(3);
  CHECK(capture.text("Before. The traveller watched the traveller disappear. After."));
  CHECK(capture.finish());
  CHECK(extract(5));
  CHECK(context.count == 6 && context.selected == 4);
  CHECK(strcmp(context.text + context.offsets[1], "traveller") == 0);
  CHECK(strcmp(context.text + context.offsets[4], "traveller") == 0);
  CHECK(capture.tokens[5].startOffset == 34 && capture.tokens[5].spineIndex == 3);
  CHECK(!context.leading && !context.trailing);
  CHECK(fit());
  CHECK(placement.first <= context.selected && placement.last > context.selected);
  CHECK(extract(6));  // Pause on the sentence-ending word keeps the preceding sentence.
  CHECK(context.selected == 5 && context.count == 6);
  CHECK(extract(7));
  CHECK(context.selected == 0 && context.count == 1);
  CHECK(extract(0));
  CHECK(context.count == 1 && context.selected == 0);

  static constexpr const char* CASES[] = {"“Don't go,” she said. “Stay!”",
                                          "I'm sure John's coming. Next.",
                                          "Dr. Smith paid 3.14 dollars. Next.",
                                          "Wait... Then leave.",
                                          "Wait… Then leave.",
                                          "(Hello!) Next.",
                                          "U.S. officials met Mr. Jones. Next."};
  static constexpr unsigned COUNTS[] = {4, 4, 5, 1, 1, 1, 5};
  for (unsigned i = 0; i < sizeof(CASES) / sizeof(CASES[0]); ++i) {
    capture.reset();
    CHECK(capture.text(CASES[i]));
    CHECK(capture.finish());
    CHECK(extract(0));
    CHECK(context.count == COUNTS[i]);
  }
  // The real parser sends decoded entities and contiguous inline fragments.
  // <p>The <em>traveller</em> &amp; <b>traveller.</b></p>
  capture.reset(7);
  CHECK(capture.text("The ", 0));
  CHECK(capture.text("trav", 4));
  CHECK(capture.text("eller", 8));
  CHECK(capture.text(" & ", 13));
  CHECK(capture.text("traveller.", 16));
  CHECK(capture.tokenizer.boundary(true));
  CHECK(capture.text("New paragraph", 26));
  CHECK(capture.finish());
  CHECK(extract(3));
  CHECK(context.count == 4 && context.selected == 3);
  CHECK(capture.tokens[3].startOffset == 16 && capture.tokens[3].endOffset == 26);
  CHECK(strcmp(context.text + context.offsets[2], "&") == 0);
  CHECK(extract(4));
  CHECK(context.count == 2 && context.selected == 0);

  capture.reset();
  CHECK(capture.finish());
  scratch = {};
  CHECK(!rsvp::extractContext(context, scratch, &capture, Capture::read, 0, scratch));
  CHECK(!context.count && !fit());
  capture.reset();
  CHECK(capture.text("Safe selected word."));
  CHECK(capture.finish());
  capture.badRecord = capture.tokens[1].fileOffset;
  CHECK(!extract(1));
  CHECK(context.count == 1 && context.selected == 0);
  CHECK(strcmp(context.text, "selected") == 0);
  capture.badRecord = UINT32_MAX;
  capture.tokens[1].previousFileOffset = capture.tokens[1].fileOffset;
  CHECK(!extract(1));
  CHECK(context.count == 1);
  return 0;
}

int bounds() {
  capture.reset();
  for (unsigned n = 0; n < 120; ++n) CHECK(capture.text("word ", n * 5));
  CHECK(capture.finish());
  CHECK(extract(60));
  CHECK(context.count == 49 && context.selected == 24 && context.leading && context.trailing);
  CHECK(capture.reads <= 76 && context.used <= rsvp::Context::TEXT_BYTES);
  CHECK(fit(200, 3));
  CHECK(placement.leading && placement.trailing);
  CHECK(placement.first < context.selected && placement.last > context.selected + 1);
  CHECK(placement.lines <= 3);
  capture.reset();
  for (unsigned n = 0; n < 100; ++n) CHECK(capture.text("extraordinary ", n * 14));
  CHECK(capture.finish());
  CHECK(extract(50));
  CHECK(fit());
  CHECK(placement.leading && placement.trailing);
  CHECK(context.selected >= placement.first && context.selected < placement.last);
  // At each chapter edge the available neighbouring text remains bounded.
  CHECK(extract(0));
  CHECK(!context.leading && context.selected == 0);
  CHECK(extract(99));
  CHECK(!context.trailing && context.selected == context.count - 1);
  // Multi-byte, combining, variation-selector, ZWJ, modifier and flag clusters.
  CHECK(rsvp::previousCluster("été", 5) == 3);
  CHECK(rsvp::previousCluster("aé", 4) == 1);
  CHECK(rsvp::previousCluster("a👩‍💻", strlen("a👩‍💻")) == 1);
  CHECK(rsvp::previousCluster("a🇦🇺", strlen("a🇦🇺")) == 1);
  CHECK(rsvp::previousCluster("a✈️", strlen("a✈️")) == 1);
  capture.reset();
  for (unsigned i = 0; i < 50; ++i) CHECK(capture.text("é", i));
  CHECK(capture.finish());
  CHECK(extract(0));
  CHECK(fit(100, 3));
  CHECK(placement.shortened && placement.selectedBytes % 2 == 0 && placement.selectedBytes < 100);
  CHECK(!fit(0, 3));
  CHECK(!fit(100, 0));
  return 0;
}
}  // namespace

extern "C" {
uint32_t rasterMeta[20];
uint32_t rasterXY[rsvp::Context::MAX_WORDS * 2];
char rasterWords[rsvp::Context::MAX_WORDS][RsvpToken::MAX_TEXT_BYTES + 1];
int rasterCase(unsigned which) {
  capture.reset();
  unsigned selected = 2;
  switch (which) {
    case 0:
      capture.text("The morning light filled the room.");
      break;
    case 1:
      capture.text("The traveller watched the traveller disappear.");
      selected = 4;
      break;
    case 2:
      capture.text("“Don't go,” she whispered, “the road is dangerous.”");
      selected = 5;
      break;
    case 3:
      capture.text(
          "The old house stood beside the river where generations of travellers had stopped to rest and listen to the "
          "sound of water flowing quietly past the garden before continuing their journey towards the distant hills.");
      selected = 16;
      break;
    default:
      for (unsigned i = 0; i < 100; ++i) capture.text("extraordinary ", i * 14);
      selected = 50;
      break;
  }
  if (!capture.finish() || !extract(selected) || !fit()) return 1;
  const uint32_t values[] = {context.count,
                             context.selected,
                             placement.first,
                             placement.last,
                             placement.lines,
                             placement.leading,
                             placement.trailing,
                             placement.shortened,
                             placement.selectedBytes,
                             context.used,
                             sizeof(context),
                             sizeof(placement),
                             sizeof(RsvpToken),
                             53,
                             static_cast<uint32_t>(fp4::toPixel(font.getGlyph(' ')->advanceX) + 2),
                             456,
                             static_cast<uint32_t>(placement.leadingX),
                             static_cast<uint32_t>(placement.trailingX),
                             placement.trailingRow};
  memcpy(rasterMeta, values, sizeof(values));
  for (unsigned i = 0; i < context.count; ++i) {
    const char* word = context.text + context.offsets[i];
    memcpy(rasterWords[i], word, strlen(word) + 1);
    rasterXY[2 * i] = placement.x[i];
    rasterXY[2 * i + 1] = placement.row[i];
  }
  return 0;
}
int testMain() {
  new (&capture) Capture;
  font.data = &notosans_16_regular;
  int failure = extraction();
  if (failure) return failure;
  failure = bounds();
  if (failure) return failure;
  for (unsigned i = 0; i < 5; ++i)
    if (rasterCase(i)) return __LINE__;
  return 0;
}
}
#ifndef RSVP_FREESTANDING
int main() { return testMain(); }
#endif
