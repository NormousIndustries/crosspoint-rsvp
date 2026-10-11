#pragma once

#include <Epub.h>
#include <Epub/Page.h>
#include <Epub/parsers/ChapterHtmlSlimParser.h>
#include <HalStorage.h>

#include "RsvpContext.h"
#include "RsvpTokenizer.h"

// Chapter text is spooled to SD by the existing parser; RAM never scales with book size.
class RsvpEpubSource {
 public:
  enum class BuildResult { More, Ready, Error };
  enum class ReadResult { Token, ChapterEnd, Error };
  RsvpEpubSource(std::shared_ptr<Epub> epub, bool embeddedStyle)
      : epub(std::move(epub)), tokenizer(this, emitToken), embeddedStyle(embeddedStyle) {}
  ~RsvpEpubSource() { close(); }
  RsvpEpubSource(const RsvpEpubSource&) = delete;
  RsvpEpubSource& operator=(const RsvpEpubSource&) = delete;
  RsvpEpubSource(RsvpEpubSource&&) = delete;
  RsvpEpubSource& operator=(RsvpEpubSource&&) = delete;
  bool beginChapter(int spineIndex, uint32_t offset, GfxRenderer& renderer);
  BuildResult buildSome();
  ReadResult next(RsvpToken& token);
  bool context(const RsvpToken& selected, rsvp::Context& out);
  void close();
  int chapter() const { return spineIndex; }
  bool hasNextChapter() const { return spineIndex + 1 < epub->getSpineItemsCount(); }
  uint32_t truncations() const { return truncatedTokens; }

 private:
  static bool consumeText(void* context, const char* text, size_t size, uint32_t offset);
  static bool emitToken(void* context, const RsvpToken& token);
  static bool readContextToken(void* context, uint32_t position, RsvpToken& token);
  static bool readRecord(HalFile& file, int chapter, RsvpToken& token);
  bool flushOutput();
  bool writeBytes(const void* bytes, size_t size);
  void releaseTokenFile();
  std::shared_ptr<Epub> epub;
  std::unique_ptr<ChapterHtmlSlimParser> parser;
  RsvpTokenizer tokenizer;
  HalFile tokenFile;
  HalFile contextFile;
  RsvpToken contextScratch;
  int contextChapter = -1;
  int firstChapter = -1;
  int lastChapter = -1;
  std::string htmlPath;
  std::string tokenPath;
  std::string extractedPath;
  uint8_t output[1024]{};
  size_t outputSize = 0;
  int spineIndex = 0;
  uint32_t startOffset = 0;
  uint32_t truncatedTokens = 0;
  uint32_t previousEndOffset = 0;
  uint32_t writtenBytes = 0;
  uint32_t previousRecord = UINT32_MAX;
  uint32_t playbackStart = UINT32_MAX;
  bool embeddedStyle;
  CssParser* visibilityCss = nullptr;
};
