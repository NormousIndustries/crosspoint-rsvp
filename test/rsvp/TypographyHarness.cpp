#include <RsvpTypography.h>
#include <Utf8.h>

#include <algorithm>

#ifdef RSVP_FREESTANDING
extern "C" void* memset(void* dest, int value, size_t size) {
  auto* p = static_cast<unsigned char*>(dest);
  for (size_t i = 0; i < size; ++i) p[i] = value;
  return dest;
}
#endif

#define CHECK(condition)               \
  do {                                 \
    if (!(condition)) return __LINE__; \
  } while (false)

namespace {
constexpr rsvp::FontSize TIERS[] = {rsvp::FontSize::Normal, rsvp::FontSize::Long, rsvp::FontSize::Compact,
                                    rsvp::FontSize::Small, rsvp::FontSize::ExtraLong};
constexpr const char* WORDS[] = {"the",
                                 "shadowless",
                                 "entrepreneur",
                                 "entrepreneurs",
                                 "extraordinary",
                                 "international",
                                 "characteristically",
                                 "\"entrepreneurs,\"",
                                 "WWWWMMM",
                                 "WMWMWMWMWM",
                                 "incomprehensibility",
                                 "iiiiiiiiiiiii",
                                 "\xE2\x80\x9C"
                                 "entrepreneurs,"
                                 "\xE2\x80\x9D",
                                 "e\xCC\x81lan",
                                 "\xC3\x85\xCC\x82lan",
                                 "\xE4\xB8\xAD\xE6\x96\x87",
                                 "AV",
                                 "gyp",
                                 "...",
                                 "well-known"};
constexpr int SIZES[] = {132, 96, 64, 92, 80};
constexpr int FOCUS = 311, BASELINE = 311, LEFT = 33, RIGHT = 765, TOP = -158, BOTTOM = 36;

bool fits(const rsvp::WordMetrics& m, int x, int left = LEFT, int right = RIGHT, int top = TOP, int bottom = BOTTOM) {
  return x + m.left >= left && x + m.right <= right && m.top >= top && m.bottom <= bottom;
}

rsvp::Placement legacy(const char* word, const rsvp::Orp& orp) {
  rsvp::Placement p;
  for (const auto tier : {rsvp::FontSize::Normal, rsvp::FontSize::Long, rsvp::FontSize::ExtraLong}) {
    p.size = tier;
    p.metrics = rsvp::walk(rsvp::font(tier), word, orp.byteOffset);
    p.x = FOCUS - (p.metrics.orpCenter2 + 1) / 2;
    p.fits = fits(p.metrics, p.x);
    if (p.fits) break;
  }
  return p;
}

struct Ink {
  bool have = false;
  int left = 0, right = 0, top = 0, bottom = 0;
};
void collect(void* ctx, const EpdGlyph& glyph, const uint8_t*, int x, int y) {
  auto& ink = *static_cast<Ink*>(ctx);
  const int left = x + glyph.left, right = left + glyph.width;
  const int top = y - glyph.top, bottom = top + glyph.height;
  ink.left = ink.have ? std::min(ink.left, left) : left;
  ink.right = ink.have ? std::max(ink.right, right) : right;
  ink.top = ink.have ? std::min(ink.top, top) : top;
  ink.bottom = ink.have ? std::max(ink.bottom, bottom) : bottom;
  ink.have = true;
}

int verify(const char* word) {
  const auto orp = rsvp::findOrp(word);
  const auto selected = rsvp::place(word, orp, FOCUS, LEFT, RIGHT, TOP, BOTTOM);
  bool found = false;
  for (const auto tier : TIERS) {
    Ink ink;
    const auto measured = rsvp::walk(rsvp::font(tier), word, orp.byteOffset, &ink, collect);
    CHECK(measured.left == ink.left && measured.right == ink.right && measured.top == ink.top &&
          measured.bottom == ink.bottom);
    const int x = FOCUS - (measured.orpCenter2 + 1) / 2;
    if (!found && fits(measured, x)) {
      CHECK(selected.size == tier && selected.fits && selected.x == x);
      found = true;
    }
  }
  CHECK(selected.fits == found);
  CHECK(2 * selected.x + selected.metrics.orpCenter2 >= 2 * FOCUS - 1);
  CHECK(2 * selected.x + selected.metrics.orpCenter2 <= 2 * FOCUS);
  if (selected.fits) CHECK(fits(selected.metrics, selected.x));
  return 0;
}
}  // namespace

extern "C" {
int rasterMeta[18];
int rasterGlyphs[200 * 3];
int rasterCandidates[5 * 7];
char rasterWord[201];
}

extern "C" int rasterCase(unsigned index) {
  CHECK(index < sizeof(WORDS) / sizeof(WORDS[0]));
  const char* word = WORDS[index];
  size_t bytes = 0;
  while (word[bytes]) {
    rasterWord[bytes] = word[bytes];
    ++bytes;
  }
  rasterWord[bytes] = '\0';
  const auto orp = rsvp::findOrp(word);
  const auto p = rsvp::place(word, orp, FOCUS, LEFT, RIGHT, TOP, BOTTOM);
  const auto old = legacy(word, orp);
  const auto face = rsvp::font(p.size);
  rasterMeta[0] = SIZES[static_cast<unsigned>(p.size)];
  rasterMeta[1] = face.getGlyph('H')->height;
  rasterMeta[2] = p.x;
  rasterMeta[3] = p.x + p.metrics.left;
  rasterMeta[4] = BASELINE + p.metrics.top;
  rasterMeta[5] = p.x + p.metrics.right;
  rasterMeta[6] = BASELINE + p.metrics.bottom;
  rasterMeta[7] = 2 * p.x + p.metrics.orpCenter2;
  rasterMeta[8] = BASELINE;
  rasterMeta[9] = orp.byteOffset;
  rasterMeta[10] = p.fits;
  rasterMeta[11] = SIZES[static_cast<unsigned>(old.size)];
  rasterMeta[12] = rsvp::font(old.size).getGlyph('H')->height;
  rasterMeta[13] = old.fits;
  rasterMeta[14] = 0;
  struct Capture {
    const EpdGlyph* first;
  } capture{face.data->glyph};
  rsvp::walk(face, word, orp.byteOffset, &capture, [](void* ctx, const EpdGlyph& glyph, const uint8_t*, int x, int y) {
    const auto& c = *static_cast<Capture*>(ctx);
    const int n = rasterMeta[14]++;
    rasterGlyphs[n * 3] = &glyph - c.first;
    rasterGlyphs[n * 3 + 1] = x;
    rasterGlyphs[n * 3 + 2] = y;
  });
  for (unsigned i = 0; i < 5; ++i) {
    const auto m = rsvp::walk(rsvp::font(TIERS[i]), word, orp.byteOffset);
    const int x = FOCUS - (m.orpCenter2 + 1) / 2;
    rasterCandidates[i * 7] = SIZES[static_cast<unsigned>(TIERS[i])];
    rasterCandidates[i * 7 + 1] = x + m.left;
    rasterCandidates[i * 7 + 2] = BASELINE + m.top;
    rasterCandidates[i * 7 + 3] = x + m.right;
    rasterCandidates[i * 7 + 4] = BASELINE + m.bottom;
    rasterCandidates[i * 7 + 5] = fits(m, x);
    rasterCandidates[i * 7 + 6] = 2 * x + m.orpCenter2;
  }
  return verify(word);
}

extern "C" int testMain() {
  for (const auto* word : WORDS) {
    const int result = verify(word);
    if (result) return result;
  }
  const auto pluralOrp = rsvp::findOrp("entrepreneurs");
  const auto plural = rsvp::place("entrepreneurs", pluralOrp, FOCUS, LEFT, RIGHT, TOP, BOTTOM);
  CHECK(plural.size == rsvp::FontSize::Compact && plural.x + plural.metrics.right == RIGHT);
  CHECK(rsvp::place("entrepreneurs", pluralOrp, FOCUS, LEFT, RIGHT - 1, TOP, BOTTOM).size == rsvp::FontSize::Small);
  CHECK(rsvp::place("iiiiiiiiiiiii", rsvp::findOrp("iiiiiiiiiiiii"), FOCUS, LEFT, RIGHT, TOP, BOTTOM).size ==
        rsvp::FontSize::Normal);
  CHECK(rsvp::findOrp("\"entrepreneurs,\"").byteOffset == pluralOrp.byteOffset + 1);
  CHECK(rsvp::findOrp("e\xCC\x81lan").byteOffset == 3);
  static char generated[201];
  constexpr const char* PATTERNS[] = {"W", "M", "i", "MWi", "AV", "gj"};
  for (const auto* pattern : PATTERNS) {
    size_t length = 0;
    while (pattern[length]) ++length;
    for (unsigned n = 1; n <= 40; ++n) {
      for (unsigned i = 0; i < n; ++i) generated[i] = pattern[i % length];
      generated[n] = '\0';
      const int result = verify(generated);
      if (result) return result;
    }
  }
  for (unsigned i = 0; i < 200; ++i) generated[i] = 'W';
  generated[200] = '\0';
  CHECK(!rsvp::place(generated, rsvp::findOrp(generated), FOCUS, LEFT, RIGHT, TOP, BOTTOM).fits);
  const auto orp = rsvp::findOrp("the");
  const auto word = rsvp::place("the", orp, FOCUS, LEFT, RIGHT, TOP, BOTTOM);
  CHECK(rsvp::place("the", orp, FOCUS, LEFT, RIGHT, word.metrics.top + 1, BOTTOM).size != rsvp::FontSize::Normal);
  CHECK(rsvp::place("gyp", rsvp::findOrp("gyp"), FOCUS, LEFT, RIGHT, TOP, 0).fits == false);
  return 0;
}
