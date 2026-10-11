#include "RsvpEpubSource.h"

#include <Logging.h>
#include <Memory.h>

bool RsvpEpubSource::beginChapter(int chapterIndex, uint32_t offset, GfxRenderer& renderer) {
  if (chapterIndex < 0 || chapterIndex >= epub->getSpineItemsCount()) {
    LOG_ERR("RSVP1", "Invalid chapter index: %d", chapterIndex);
    return false;
  }
  parser.reset();
  releaseTokenFile();
  outputSize = 0;
  previousEndOffset = 0;
  spineIndex = chapterIndex;
  startOffset = offset;
  LOG_INF("RSVP1", "CHAPTER load index=%d offset=%u", spineIndex, startOffset);
  tokenizer.reset(spineIndex);
  tokenPath = epub->getCachePath() + "/rsvp-v1a.tmp";
  extractedPath = epub->getCachePath() + "/rsvp-v1a.html.tmp";
  htmlPath = epub->getCachePath() + "/html/" + std::to_string(spineIndex) + ".html";
  if (!Storage.exists(htmlPath.c_str())) {
    htmlPath = extractedPath;
    if (!epub->extractItemToFile(epub->getSpineItem(spineIndex).href, htmlPath)) {
      LOG_ERR("RSVP1", "Chapter extraction failed: %d", spineIndex);
      return false;
    }
  }
  if (!Storage.openFileForWrite("RSVP1", tokenPath, tokenFile)) return false;
  LOG_DBG("RSVP1", "FILE open mode=write chapter=%d", spineIndex);
  visibilityCss = embeddedStyle ? epub->getCssParser() : nullptr;
  if (visibilityCss) {
    const auto result = visibilityCss->loadFromCache();
    if (result == CssParser::CacheLoadResult::LowMemory) {
      LOG_ERR("RSVP1", "OOM: CSS visibility cache");
      return false;
    }
    if (result == CssParser::CacheLoadResult::Invalid) {
      LOG_ERR("RSVP1", "CSS visibility cache invalid; using available rules/inline visibility");
    }
  }
  // Text-only mode allocates neither ParsedText/WordStore nor pages/images.
  parser = makeUniqueNoThrow<ChapterHtmlSlimParser>(epub, htmlPath, renderer, 0, 1.0f, false, 0, 0, 0, false,
                                                  false, nullptr, embeddedStyle, "", "", 0,
                                                  std::vector<std::string>{}, nullptr, visibilityCss);
  if (!parser) {
    LOG_ERR("RSVP1", "OOM: chapter text parser");
    return false;
  }
  parser->setVisibleTextSink({this, consumeText});
  return parser->beginParse();
}

bool RsvpEpubSource::consumeText(void* context, const char* text, size_t size, uint32_t offset) {
  auto& self = *static_cast<RsvpEpubSource*>(context);
  return text ? self.tokenizer.consume(text, size, offset) : self.tokenizer.boundary();
}

bool RsvpEpubSource::emitToken(void* context, const RsvpToken& token) {
  auto& self = *static_cast<RsvpEpubSource*>(context);
  if (token.endOffset <= self.startOffset) return true;
  return self.writeBytes(&token.startOffset, sizeof(token.startOffset)) &&
         self.writeBytes(&token.endOffset, sizeof(token.endOffset)) &&
         self.writeBytes(&token.length, sizeof(token.length)) && self.writeBytes(token.text, token.length);
}

bool RsvpEpubSource::flushOutput() {
  if (outputSize && tokenFile.write(output, outputSize) != outputSize) {
    LOG_ERR("RSVP1", "Token spool write failed");
    return false;
  }
  outputSize = 0;
  return true;
}

bool RsvpEpubSource::writeBytes(const void* bytes, size_t size) {
  if (outputSize + size > sizeof(output) && !flushOutput()) return false;
  memcpy(output + outputSize, bytes, size);
  outputSize += size;
  return true;
}

RsvpEpubSource::BuildResult RsvpEpubSource::buildSome() {
  if (!parser) {
    LOG_ERR("RSVP1", "No chapter parser");
    return BuildResult::Error;
  }
  // Four 1KB parse steps per render tick bound loading work between control checks.
  for (int i = 0; i < 4; ++i) {
    const auto status = parser->parseStep();
    if (status == ChapterHtmlSlimParser::ParseStatus::Error) return BuildResult::Error;
    if (status != ChapterHtmlSlimParser::ParseStatus::Done) continue;
    if (!parser->finishParse() || !flushOutput()) return BuildResult::Error;
    truncatedTokens += tokenizer.truncations();
    parser.reset();
    if (visibilityCss) visibilityCss->clear();
    releaseTokenFile();
    if (!Storage.openFileForRead("RSVP1", tokenPath, tokenFile)) return BuildResult::Error;
    LOG_DBG("RSVP1", "FILE open mode=read chapter=%d", spineIndex);
    return BuildResult::Ready;
  }
  return BuildResult::More;
}

RsvpEpubSource::ReadResult RsvpEpubSource::next(RsvpToken& token) {
  if (!tokenFile) {
    LOG_ERR("RSVP1", "No token spool");
    return ReadResult::Error;
  }
  if (tokenFile.position() == tokenFile.size()) return ReadResult::ChapterEnd;
  token.spineIndex = spineIndex;
  // Read into aligned fields rather than dereferencing a packed SD buffer.
  if (tokenFile.read(&token.startOffset, sizeof(token.startOffset)) != sizeof(token.startOffset) ||
      tokenFile.read(&token.endOffset, sizeof(token.endOffset)) != sizeof(token.endOffset) ||
      tokenFile.read(&token.length, sizeof(token.length)) != sizeof(token.length) || !token.length ||
      token.length > RsvpToken::MAX_TEXT_BYTES || token.startOffset < previousEndOffset ||
      token.endOffset <= token.startOffset || tokenFile.read(token.text, token.length) != token.length) {
    LOG_ERR("RSVP1", "Invalid/truncated token spool");
    return ReadResult::Error;
  }
  token.text[token.length] = '\0';
  previousEndOffset = token.endOffset;
  return ReadResult::Token;
}

void RsvpEpubSource::releaseTokenFile() {
  if (tokenFile.isOpen()) LOG_DBG("RSVP1", "FILE release chapter=%d", spineIndex);
  // Move assignment releases the owned Impl under StorageLock, including an empty slot.
  tokenFile = HalFile{};
}

void RsvpEpubSource::close() {
  parser.reset();
  if (visibilityCss) visibilityCss->clear();
  releaseTokenFile();
  if (!tokenPath.empty() && Storage.exists(tokenPath.c_str())) Storage.remove(tokenPath.c_str());
  if (!extractedPath.empty() && Storage.exists(extractedPath.c_str())) Storage.remove(extractedPath.c_str());
}
