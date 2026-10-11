#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Rsvp/RsvpCadence.h"
#include "Rsvp/RsvpTokenizer.h"

namespace {
struct Capture {
  std::vector<RsvpToken> tokens;
  RsvpTokenizer tokenizer{this, [](void* context, const RsvpToken& token) {
                            static_cast<Capture*>(context)->tokens.push_back(token);
                            return true;
                          }};
  Capture() {
    tokens.reserve(64);
    tokenizer.reset(3);
  }
};
}

TEST(RsvpTokenizer, PreservesWordsAndUnicodeOffsetsAcrossChunks) {
  Capture capture;
  ASSERT_TRUE(capture.tokenizer.consume("don't well-", 11, 0));
  ASSERT_TRUE(capture.tokenizer.consume("known\xC2\xA0morning, \xC3", 17, 11));
  ASSERT_TRUE(capture.tokenizer.consume("\xA9lan", 4, 27));
  ASSERT_TRUE(capture.tokenizer.boundary());
  ASSERT_EQ(capture.tokens.size(), 4u);
  EXPECT_STREQ(capture.tokens[0].text, "don't");
  EXPECT_STREQ(capture.tokens[1].text, "well-known");
  EXPECT_STREQ(capture.tokens[2].text, "morning,");
  EXPECT_STREQ(capture.tokens[3].text, "\xC3\xA9lan");
  EXPECT_EQ(capture.tokens[0].startOffset, 0u);
  EXPECT_EQ(capture.tokens[1].startOffset, 6u);
  EXPECT_EQ(capture.tokens[1].endOffset, 16u);
  EXPECT_EQ(capture.tokens[2].startOffset, 17u);
  EXPECT_EQ(capture.tokens[3].startOffset, 26u);
  EXPECT_EQ(capture.tokens[3].endOffset, 30u);
  EXPECT_EQ(capture.tokens[3].spineIndex, 3);
}

TEST(RsvpTokenizer, LongTokenTruncatesAtCodepointBoundaryWithoutExtraToken) {
  Capture capture;
  const std::string text(199, 'a');
  ASSERT_TRUE(capture.tokenizer.consume(text.c_str(), text.size(), 0));
  ASSERT_TRUE(capture.tokenizer.consume("\xC3\xA9tail ", 7, 199));
  ASSERT_TRUE(capture.tokenizer.boundary());
  ASSERT_EQ(capture.tokens.size(), 1u);
  EXPECT_EQ(capture.tokens[0].length, 199u);
  EXPECT_EQ(capture.tokens[0].endOffset, 204u);
  EXPECT_EQ(capture.tokenizer.truncations(), 1u);
}

TEST(RsvpTokenizer, EmptyTextAndWhitespaceEmitNothing) {
  Capture capture;
  ASSERT_TRUE(capture.tokenizer.consume(" \t\r\n\xE3\x80\x80", 7, 0));
  ASSERT_TRUE(capture.tokenizer.boundary());
  EXPECT_TRUE(capture.tokens.empty());
}

TEST(RsvpTokenizer, OutputFailureIsPropagated) {
  RsvpTokenizer tokenizer{nullptr, [](void*, const RsvpToken&) { return false; }};
  tokenizer.reset(0);
  ASSERT_TRUE(tokenizer.consume("word ", 5, 0));
  EXPECT_FALSE(tokenizer.boundary());
}

TEST(RsvpHistory, ChapterBoundaryReplayAndEvictionStayBounded) {
  RsvpHistory history;
  RsvpToken token;
  for (uint32_t i = 0; i < 1000; ++i) {
    token.spineIndex = i < 999 ? 0 : 1;
    token.startOffset = i;
    history.append(token);
  }
  ASSERT_TRUE(history.peekPrevious());
  EXPECT_EQ(history.peekPrevious()->spineIndex, 0);
  ASSERT_TRUE(history.previous());
  EXPECT_EQ(history.current()->startOffset, 998u);
  ASSERT_TRUE(history.replayNext());
  EXPECT_EQ(history.current()->spineIndex, 1);
  for (size_t i = 1; i < RsvpHistory::CAPACITY; ++i) ASSERT_TRUE(history.previous());
  EXPECT_FALSE(history.previous());
  EXPECT_EQ(history.current()->startOffset, 968u);
  for (size_t i = 1; i < RsvpHistory::CAPACITY; ++i) ASSERT_TRUE(history.replayNext());
  EXPECT_FALSE(history.replayNext());
  EXPECT_EQ(history.current()->startOffset, 999u);
}

TEST(RsvpCadence, OverrunRebasesWithoutCatchupSlots) {
  RsvpCadence cadence;
  EXPECT_EQ(cadence.wpm(), 100);
  EXPECT_EQ(cadence.intervalUs(), 600000);
  cadence.faster();
  EXPECT_EQ(cadence.wpm(), 125);
  EXPECT_EQ(cadence.intervalUs(), 480000);
  cadence.completed(1000000, 1500000);
  EXPECT_EQ(cadence.wordsPresented, 1u);
  EXPECT_EQ(cadence.deadlineMisses, 1u);
  EXPECT_TRUE(cadence.due(1500000));
  cadence.completed(1501000, 2001000);
  EXPECT_EQ(cadence.wordsPresented, 2u);
  EXPECT_EQ(cadence.deadlineMisses, 2u);
  EXPECT_EQ(cadence.effectiveWpm(), 119u);
  cadence.slower();
  EXPECT_EQ(cadence.wpm(), 100);
  cadence.rebase(2001000);
  EXPECT_FALSE(cadence.due(2600999));
  EXPECT_TRUE(cadence.due(2601000));
  cadence.slower();
  EXPECT_EQ(cadence.wpm(), 75);
  cadence.slower();
  cadence.slower();
  EXPECT_EQ(cadence.wpm(), 50);
  for (int i = 0; i < 5; ++i) cadence.faster();
  EXPECT_EQ(cadence.wpm(), 125);
}
