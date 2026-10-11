#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "Rsvp/RsvpTypography.h"

TEST(RsvpOrp, LexicalRuleBoundaries) {
  EXPECT_EQ(rsvp::orpIndex(1), 0);
  EXPECT_EQ(rsvp::orpIndex(2), 1);
  EXPECT_EQ(rsvp::orpIndex(5), 1);
  EXPECT_EQ(rsvp::orpIndex(6), 2);
  EXPECT_EQ(rsvp::orpIndex(9), 2);
  EXPECT_EQ(rsvp::orpIndex(10), 3);
  EXPECT_EQ(rsvp::orpIndex(13), 3);
  EXPECT_EQ(rsvp::orpIndex(14), 4);
}

TEST(RsvpOrp, QuotesTrailingPunctuationAndConnectors) {
  EXPECT_EQ(rsvp::findOrp("I").byteOffset, 0u);
  EXPECT_EQ(rsvp::findOrp("the").byteOffset, 1u);
  EXPECT_EQ(rsvp::findOrp("morning,").byteOffset, 2u);
  EXPECT_EQ(rsvp::findOrp("something").byteOffset, 2u);
  EXPECT_EQ(rsvp::findOrp("extraordinary").byteOffset, 3u);
  EXPECT_EQ(rsvp::findOrp("\"perhaps\"").byteOffset, 3u);
  EXPECT_EQ(rsvp::findOrp("(perhaps)").byteOffset, 3u);
  EXPECT_EQ(rsvp::findOrp("don't").lexicalLength, 4u);
  EXPECT_EQ(rsvp::findOrp("well-known").lexicalLength, 9u);
  EXPECT_EQ(rsvp::findOrp("well-known").byteOffset, 2u);
  EXPECT_EQ(rsvp::findOrp("\xE2\x80\x9Cthe\xE2\x80\x9D").byteOffset, 4u);
  EXPECT_EQ(rsvp::findOrp("\xC3\xA9lan").byteOffset, 2u);
  EXPECT_EQ(rsvp::findOrp("e\xCC\x81lan").byteOffset, 3u);
  EXPECT_EQ(rsvp::findOrp("...").byteOffset, 0u);
}

TEST(RsvpTypography, ActualEmbeddedPixelMetricsAndKerning) {
  const auto face = rsvp::font(rsvp::FontSize::Normal);
  ASSERT_NE(face.getGlyph('H'), nullptr);
  EXPECT_EQ(face.getGlyph('H')->height, 94);
  EXPECT_EQ(face.getGlyph('x')->height, 71);
  EXPECT_FALSE(face.data->is2Bit);
  EXPECT_EQ(face.data->groups, nullptr);
  EXPECT_EQ(face.data->glyphMissHandler, nullptr);
  EXPECT_EQ(face.data->ligaturePairCount, 0u);
  EXPECT_LT(face.getKerning('A', 'V'), 0);
}

TEST(RsvpTypography, FocalColumnIsInvariantAcrossRealGlyphs) {
  static constexpr const char* WORDS[] = {"I", "an", "the", "reading", "morning,", "something", "extraordinary",
                                        "\"perhaps\"", "well-known", "characteristically", "AV", "gyp"};
  for (const auto* word : WORDS) {
    SCOPED_TRACE(word);
    const auto orp = rsvp::findOrp(word);
    const auto placement = rsvp::place(word, orp, 306, 24, 768);
    ASSERT_TRUE(placement.fits);
    const int center2 = 2 * placement.x + placement.metrics.orpCenter2;
    EXPECT_GE(center2, 611);
    EXPECT_LE(center2, 612);
    EXPECT_GE(placement.x + placement.metrics.left, 24);
    EXPECT_LE(placement.x + placement.metrics.right, 768);
  }
}

TEST(RsvpTypography, CommonWordsKeepNormalAndLongWordsUseDiscreteFallback) {
  for (const auto* word : {"reading", "morning,", "something", "perhaps", "Hello,"}) {
    SCOPED_TRACE(word);
    EXPECT_EQ(rsvp::place(word, rsvp::findOrp(word), 306, 24, 768).size, rsvp::FontSize::Normal);
  }
  const char* word = "characteristically";
  const auto placement = rsvp::place(word, rsvp::findOrp(word), 306, 24, 768);
  EXPECT_TRUE(placement.fits);
  EXPECT_NE(placement.size, rsvp::FontSize::Normal);
  const std::string huge(200, 'W');
  EXPECT_FALSE(rsvp::place(huge.c_str(), rsvp::findOrp(huge.c_str()), 306, 24, 768).fits);
}

TEST(RsvpTypography, SelectedGlyphDrawLocationMatchesMeasuredInkCenter) {
  const char* word = "\"AV\"";
  const auto orp = rsvp::findOrp(word);
  const auto face = rsvp::font(rsvp::FontSize::Normal);
  struct Capture {
    const EpdGlyph* target;
    int center2 = -1;
  } capture{face.getGlyph('V')};
  const auto measured = rsvp::walk(face, word, orp.byteOffset, &capture,
      [](void* ctx, const EpdGlyph& glyph, const uint8_t*, int x, int) {
        auto& result = *static_cast<Capture*>(ctx);
        if (&glyph == result.target) result.center2 = 2 * (x + glyph.left) + glyph.width;
      });
  EXPECT_EQ(measured.orpCenter2, capture.center2);
}

TEST(RsvpTypography, RaisedMarksCannotClipWordRegion) {
  const char* word = "\xC3\x85\xCC\x82lan";
  const auto placement = rsvp::place(word, rsvp::findOrp(word), 306, 24, 768, -158, 36);
  ASSERT_TRUE(placement.fits);
  EXPECT_GE(placement.metrics.top, -158);
  EXPECT_LE(placement.metrics.bottom, 36);
}
