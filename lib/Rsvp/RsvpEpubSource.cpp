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
  writtenBytes = 0;
  previousRecord = UINT32_MAX;
  playbackStart = UINT32_MAX;
  spineIndex = chapterIndex;
  startOffset = offset;
  LOG_INF("RSVP1", "CHAPTER load index=%d offset=%u", spineIndex, startOffset);
  tokenizer.reset(spineIndex);
  contextFile = HalFile{};
  contextChapter = -1;
  tokenPath = epub->getCachePath() + "/rsvp-v1c2-" + std::to_string(spineIndex) + ".tmp";
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
  if (firstChapter < 0) firstChapter = spineIndex;
  lastChapter = spineIndex;
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
  parser = makeUniqueNoThrow<ChapterHtmlSlimParser>(epub, htmlPath, renderer, 0, 1.0f, false, 0, 0, 0, false, false,
                                                    nullptr, embeddedStyle, "", "", 0, std::vector<std::string>{},
                                                    nullptr, visibilityCss);
  if (!parser) {
    LOG_ERR("RSVP1", "OOM: chapter text parser");
    return false;
  }
  parser->setVisibleTextSink({this, consumeText});
  return parser->beginParse();
}

bool RsvpEpubSource::consumeText(void* context, const char* text, size_t size, uint32_t offset) {
  auto& self = *static_cast<RsvpEpubSource*>(context);
  return text   ? self.tokenizer.consume(text, size, offset)
         : size ? self.tokenizer.boundary(size == 1, size == 2)
                : self.tokenizer.wordBoundary();
}

bool RsvpEpubSource::emitToken(void* context, const RsvpToken& token) {
  auto& self = *static_cast<RsvpEpubSource*>(context);
  // Keep preceding records for positional context, even when entering mid-chapter.
  const uint8_t flags = (token.paragraphEnd ? 1 : 0) | (token.chapterEnd ? 2 : 0);
  const uint32_t position = self.writtenBytes;
  const bool success = self.writeBytes(&token.startOffset, sizeof(token.startOffset)) &&
                       self.writeBytes(&token.endOffset, sizeof(token.endOffset)) &&
                       self.writeBytes(&token.length, sizeof(token.length)) &&
                       self.writeBytes(&token.timing, sizeof(token.timing)) && self.writeBytes(&flags, sizeof(flags)) &&
                       self.writeBytes(&self.previousRecord, sizeof(self.previousRecord)) &&
                       self.writeBytes(token.text, token.length);
  if (success) {
    self.previousRecord = position;
    if (self.playbackStart == UINT32_MAX) {
      if (token.endOffset > self.startOffset)
        self.playbackStart = position;
      else
        self.previousEndOffset = token.endOffset;
    }
  }
  return success;
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
  writtenBytes += size;
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
    // Position the playback cursor once; context reads never seek this handle.
    if (!tokenFile.seek(playbackStart == UINT32_MAX ? writtenBytes : playbackStart)) {
      LOG_ERR("RSVP1", "Cannot position token spool");
      return BuildResult::Error;
    }
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
  if (!readRecord(tokenFile, spineIndex, token) || token.startOffset < previousEndOffset) {
    LOG_ERR("RSVP1", "Invalid token order");
    return ReadResult::Error;
  }
  previousEndOffset = token.endOffset;
  return ReadResult::Token;
}

bool RsvpEpubSource::readRecord(HalFile& file, int chapter, RsvpToken& token) {
  token.spineIndex = chapter;
  token.fileOffset = file.position();
  uint8_t flags = 0;
  // Read into aligned fields rather than dereferencing a packed SD buffer.
  if (file.read(&token.startOffset, sizeof(token.startOffset)) != sizeof(token.startOffset) ||
      file.read(&token.endOffset, sizeof(token.endOffset)) != sizeof(token.endOffset) ||
      file.read(&token.length, sizeof(token.length)) != sizeof(token.length) || !token.length ||
      file.read(&token.timing, sizeof(token.timing)) != sizeof(token.timing) ||
      static_cast<uint8_t>(token.timing) >= static_cast<uint8_t>(rsvp::Timing::Count) ||
      file.read(&flags, sizeof(flags)) != sizeof(flags) || flags > 3 ||
      file.read(&token.previousFileOffset, sizeof(token.previousFileOffset)) != sizeof(token.previousFileOffset) ||
      (token.previousFileOffset != UINT32_MAX && token.previousFileOffset >= token.fileOffset) ||
      token.length > RsvpToken::MAX_TEXT_BYTES || token.endOffset <= token.startOffset ||
      file.read(token.text, token.length) != token.length) {
    LOG_ERR("RSVP1", "Invalid/truncated token spool");
    return false;
  }
  token.text[token.length] = '\0';
  token.paragraphEnd = flags & 1;
  token.chapterEnd = flags & 2;
  return true;
}

bool RsvpEpubSource::readContextToken(void* context, uint32_t position, RsvpToken& token) {
  auto& self = *static_cast<RsvpEpubSource*>(context);
  return self.contextFile.seek(position) && readRecord(self.contextFile, self.contextChapter, token);
}

bool RsvpEpubSource::context(const RsvpToken& selected, rsvp::Context& out) {
  out.fallback(selected);
  if (contextChapter != selected.spineIndex || !contextFile) {
    contextFile = HalFile{};
    contextChapter = -1;
    // Chapter changes only: reuse the handle for ordinary paused navigation.
    const auto path = epub->getCachePath() + "/rsvp-v1c2-" + std::to_string(selected.spineIndex) + ".tmp";
    if (!Storage.openFileForRead("RSVP2", path, contextFile)) return false;
    contextChapter = selected.spineIndex;
  }
  return rsvp::extractContext(out, selected, this, readContextToken, contextFile.size(), contextScratch);
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
  contextFile = HalFile{};
  contextChapter = -1;
  // Retain earlier chapter spools until exit so recent-history replay has context.
  for (int chapter = firstChapter; chapter >= 0 && chapter <= lastChapter; ++chapter) {
    tokenPath = epub->getCachePath() + "/rsvp-v1c2-" + std::to_string(chapter) + ".tmp";
    if (Storage.exists(tokenPath.c_str())) Storage.remove(tokenPath.c_str());
  }
  firstChapter = lastChapter = -1;
  if (!extractedPath.empty() && Storage.exists(extractedPath.c_str())) Storage.remove(extractedPath.c_str());
}
