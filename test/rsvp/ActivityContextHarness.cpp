// Compile the actual activity with deterministic hardware/storage substitutes.
// run_context_tests.py removes includes into an ignored translation unit only.
#include <algorithm>
#include <new>
#include <utility>

#include "RsvpCadence.h"
#include "RsvpContext.h"

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
  size_t n = 0;
  while (s[n]) ++n;
  return n;
}
void operator delete(void*) noexcept {}
void operator delete(void*, size_t) noexcept {}

// Single-thread test substitutes; a submit hook models queued input while busy.
namespace std {
template <class T>
class shared_ptr {
  T* value = nullptr;

 public:
  explicit shared_ptr(T* value = nullptr) : value(value) {}
  T* operator->() const { return value; }
  T& operator*() const { return *value; }
};
template <class T>
struct atomic {
  T value;
  explicit atomic(T v) : value(v) {}
  T load() const { return value; }
  void store(T v) { value = v; }
  T exchange(T v) {
    T old = value;
    value = v;
    return old;
  }
  T fetch_or(T v) {
    T old = value;
    value |= v;
    return old;
  }
};
}  // namespace std

#define LOG_INF(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define LOG_DBG(...) ((void)0)
enum class StrId { STR_RSVP_PAUSED, STR_LOADING, STR_PAGE_LOAD_ERROR, STR_END_OF_BOOK };
#define tr(key) "status"
static int64_t now;
int64_t esp_timer_get_time() { return now; }
constexpr uint32_t MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2;
uint32_t heap_caps_get_free_size(uint32_t) { return 100000; }
uint32_t heap_caps_get_largest_free_block(uint32_t) { return 80000; }
struct MockEsp {
  uint32_t getFreeHeap() { return 100000; }
  uint32_t getMinFreeHeap() { return 90000; }
} ESP;
struct CrossPointPosition {
  int spineIndex = 0;
  uint32_t visibleTextOffset = 0;
  bool hasVisibleTextOffset = true;
};
struct ActivityResult {
  bool isCancelled = false;
};
struct ProgressChangeResult {
  int spineIndex = 0;
  uint32_t visibleTextOffset = 0;
  bool hasVisibleTextOffset = false;
};
struct MockPath {
  const char* c_str() const { return "book"; }
};
struct Epub {
  MockPath getPath() const { return {}; }
};
struct ChapterHtmlSlimParser {};
struct GfxRenderer {
  enum Orientation { Portrait, LandscapeCounterClockwise };
  enum RenderMode { BW };
  Orientation orientation = Portrait;
  Orientation getOrientation() { return orientation; }
  RenderMode getRenderMode() { return BW; }
  void setOrientation(Orientation v) { orientation = v; }
  void setRenderMode(RenderMode) {}
  void clearScreen() {}
};
struct MappedInputManager {
  enum class Button { Confirm, Left, Right, Down, Up, Back };
  uint8_t released = 0;
  bool wasReleased(Button b) {
    const auto mask = 1 << static_cast<uint8_t>(b);
    bool v = released & mask;
    released &= ~mask;
    return v;
  }
  void release(Button b) { released |= 1 << static_cast<uint8_t>(b); }
};
struct MockDisplay {
  bool isInverted() { return false; }
  void setInverted(bool) {}
} display;
struct MockGpio {
  bool deviceIsX3() { return true; }
} gpio;
struct RenderLock {
  enum class Mode { Try };
  RenderLock() = default;
  explicit RenderLock(Mode) {}
  bool ownsLock() { return true; }
};
struct Activity {
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  unsigned requests = 0;
  bool finished = false;
  ProgressChangeResult result;
  Activity(const char*, GfxRenderer& r, MappedInputManager& i) : renderer(r), mappedInput(i) {}
  virtual ~Activity() = default;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual bool preventAutoSleep() { return false; }
  virtual bool handleForcedRefresh() { return false; }
  void requestUpdate() { ++requests; }
  void finish() { finished = true; }
  void setResult(ProgressChangeResult v) { result = v; }
  void setResult(ActivityResult) {}
};
namespace rsvp {
inline constexpr uint8_t FONT_TIER_COUNT = 5;
struct Orp {
  int byteOffset = 0;
};
struct Placement {
  bool fits = true;
  int x = 0;
  uint8_t size = 0;
};
Orp findOrp(const char*) { return {}; }
Placement place(const char*, Orp, int, int, int, int, int) { return {}; }
}  // namespace rsvp
struct Rect {
  int x = 0, y = 0, width = 0, height = 0;
};
struct RsvpReaderLayout {
  Rect word;
  int focusX = 0, baseline = 0, upperTip = 0, lowerTip = 0, controlsY = 0, rowHeight = 0;
};
struct UITheme {
  unsigned wordFrames = 0, contextFrames = 0, pausedFrames = 0;
  uint32_t shownOffset = UINT32_MAX;
  static UITheme& getInstance() {
    static UITheme instance;
    return instance;
  }
  RsvpReaderLayout rsvpLayout(GfxRenderer&) { return {}; }
  void drawRsvpWord(GfxRenderer&, const RsvpReaderLayout&, const char*, rsvp::Placement) { ++wordFrames; }
  void drawRsvpContext(GfxRenderer&, const RsvpReaderLayout&, const rsvp::Context&, rsvp::ContextPlacement&) {
    ++contextFrames;
  }
  void drawRsvpPaused(GfxRenderer&, const RsvpReaderLayout&, const char*, uint16_t, bool, MappedInputManager&,
                      rsvp::Placement) {
    ++pausedFrames;
  }
};
static unsigned submissions;
static bool submitSuccess;
static void (*submitHook)();
struct RsvpDisplayBackend {
  struct Completion {
    bool success;
  };
  Completion submit() {
    ++submissions;
    now += 100000;
    if (submitHook) submitHook();
    return {submitSuccess};
  }
};
struct RsvpEpubSource {
  enum class BuildResult { More, Ready, Error };
  enum class ReadResult { Token, ChapterEnd, Error };
  RsvpToken tokens[4]{};
  unsigned cursor = 0, nextCalls = 0, contextCalls = 0;
  int loadedChapter = 0;
  bool failContext = false;
  bool allEmpty = false, emptyMiddle = false;
  int finalChapter = 1;
  RsvpEpubSource(std::shared_ptr<Epub>, bool) {
    const char* words[] = {"One", "ends.", "Next", "ends."};
    for (unsigned i = 0; i < 4; ++i) {
      auto& t = tokens[i];
      t.spineIndex = i / 2;
      t.startOffset = (i % 2) * 4;
      t.endOffset = t.startOffset + strlen(words[i]);
      t.length = strlen(words[i]);
      memcpy(t.text, words[i], t.length + 1);
      t.timing = i % 2 ? rsvp::Timing::Paragraph : rsvp::Timing::Normal;
      t.chapterEnd = i % 2;
    }
  }
  bool beginChapter(int chapter, uint32_t, GfxRenderer&) {
    loadedChapter = chapter;
    if (emptyMiddle) {
      tokens[2].spineIndex = tokens[3].spineIndex = finalChapter;
    }
    return true;
  }
  BuildResult buildSome() { return BuildResult::Ready; }
  ReadResult next(RsvpToken& t) {
    ++nextCalls;
    if (allEmpty || cursor == 4 || (cursor == 2 && loadedChapter < finalChapter)) return ReadResult::ChapterEnd;
    t = tokens[cursor++];
    return ReadResult::Token;
  }
  bool context(const RsvpToken& selected, rsvp::Context& out) {
    ++contextCalls;
    out.fallback(selected);
    return !failContext;
  }
  int chapter() { return loadedChapter; }
  bool hasNextChapter() { return loadedChapter < finalChapter; }
  uint32_t truncations() { return 0; }
  void close() {}
};
namespace EpubReaderUtils {
bool saveProgress(Epub&, int, int, int, uint32_t) { return true; }
}  // namespace EpubReaderUtils

struct CrossPointSettings {
  uint8_t rsvpWpm = 100, rsvpSmartTiming = 1;
  uint8_t persistedWpm = 100, persistedSmartTiming = 1;
  unsigned writes = 0;
  bool writeSuccess = true;
  bool saveToFile() {
    ++writes;
    if (!writeSuccess) return false;
    persistedWpm = rsvpWpm;
    persistedSmartTiming = rsvpSmartTiming;
    return true;
  }
  bool saveRsvpSpeed(uint16_t wpm);
} settings;
#define SETTINGS settings
#include "ActivityUnderTest.h"
// Keep generated definitions after their class declaration.
#include "ActivityUnderTest.cpp"
#include "SettingsSpeedUnderTest.cpp"

#define CHECK(condition)               \
  do {                                 \
    if (!(condition)) return __LINE__; \
  } while (false)
static RsvpReaderActivity* active;
static void queuePause() {
  active->controls.fetch_or(RsvpReaderActivity::TOGGLE);
  submitHook = nullptr;
}
extern "C" int testMain() {
  GfxRenderer renderer;
  MappedInputManager input;
  Epub epub;
  alignas(RsvpReaderActivity) static unsigned char memory[sizeof(RsvpReaderActivity)];
  active = new (memory) RsvpReaderActivity(renderer, input, std::shared_ptr<Epub>(&epub), {}, false);
  auto& a = *active;
  auto& ui = UITheme::getInstance();
  submissions = 0;
  submitSuccess = true;
  submitHook = nullptr;
  now = 0;
  a.onEnter();
  a.render(RenderLock{});
  a.render(RenderLock{});
  CHECK(a.state.load() == RsvpReaderActivity::State::Paused);
  CHECK(a.history.current()->startOffset == 0 && a.source.cursor == 1 && a.source.contextCalls == 1);
  CHECK(ui.wordFrames == 0 && ui.contextFrames == 1);
  const auto idleSubmissions = submissions;
  for (unsigned i = 0; i < 10; ++i) {
    a.loop();
    a.render(RenderLock{});
  }
  CHECK(submissions == idleSubmissions && a.source.cursor == 1);
  input.release(MappedInputManager::Button::Down);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.cadence.wpm() == 125 && a.source.contextCalls == 1);
  input.release(MappedInputManager::Button::Up);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.cadence.wpm() == 100);
  input.release(MappedInputManager::Button::Confirm);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.state.load() == RsvpReaderActivity::State::Playing && ui.wordFrames == 1);
  CHECK(a.history.current()->startOffset == 0 && a.source.cursor == 1 && a.cadence.wordsPresented == 1);
  now += 900000;  // Pause takes priority even if the deadline has elapsed.
  input.release(MappedInputManager::Button::Confirm);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.state.load() == RsvpReaderActivity::State::Paused && a.history.current()->startOffset == 0);
  CHECK(a.source.cursor == 1);
  const auto contexts = ui.contextFrames;
  const auto pausedLabels = ui.pausedFrames;
  input.release(MappedInputManager::Button::Confirm);
  a.loop();
  const auto start = now;
  a.render(RenderLock{});
  CHECK(a.history.current()->startOffset == 0 && a.source.cursor == 1);
  CHECK(!a.cadence.due(start + 599999) && a.cadence.due(start + 600000));
  now = start + 600000;
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->startOffset == 4 && a.source.cursor == 2 && a.cadence.wordsPresented == 2);
  CHECK(ui.contextFrames == contexts && ui.pausedFrames == pausedLabels);
  input.release(MappedInputManager::Button::Confirm);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->startOffset == 4 && a.contextOffset == 4);
  input.release(MappedInputManager::Button::Confirm);
  a.loop();
  const auto endingStart = now;
  a.render(RenderLock{});
  CHECK(!a.cadence.due(endingStart + 1099999) && a.cadence.due(endingStart + 1100000));
  // Queue pause during a display completion; selection remains that completed word.
  submitHook = queuePause;
  now = endingStart + 1100000;
  a.loop();
  a.render(RenderLock{});  // Chapter-end discovery, loading chrome next.
  a.render(RenderLock{});
  a.loop();
  a.render(RenderLock{});
  a.loop();
  a.render(RenderLock{});
  CHECK(a.state.load() == RsvpReaderActivity::State::Paused);
  CHECK(a.history.current()->spineIndex == 0 && a.history.current()->startOffset == 4);
  // Explicit Next selects the first word of the new chapter; Previous replays across it.
  input.release(MappedInputManager::Button::Right);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->spineIndex == 1 && a.history.current()->startOffset == 0);
  CHECK(a.contextSpine == 1);
  const auto nextCursor = a.source.cursor;
  input.release(MappedInputManager::Button::Left);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->spineIndex == 0 && a.history.current()->startOffset == 4 && a.source.cursor == nextCursor);
  input.release(MappedInputManager::Button::Right);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->spineIndex == 1 && a.source.cursor == nextCursor);
  a.source.failContext = true;
  input.release(MappedInputManager::Button::Right);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->startOffset == 4 && a.context.count == 1);
  input.release(MappedInputManager::Button::Right);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.endReached && a.state.load() == RsvpReaderActivity::State::Paused);
  input.release(MappedInputManager::Button::Confirm);
  a.loop();
  const auto finalStart = now;
  a.render(RenderLock{});
  CHECK(a.state.load() == RsvpReaderActivity::State::Playing && a.history.current()->startOffset == 4);
  now = finalStart + 1100000;
  a.loop();
  a.render(RenderLock{});
  CHECK(a.state.load() == RsvpReaderActivity::State::Paused && a.history.current()->startOffset == 4);
  input.release(MappedInputManager::Button::Back);
  a.loop();
  CHECK(a.finished && a.result.spineIndex == 1 && a.result.visibleTextOffset == 4 && a.result.hasVisibleTextOffset);
  a.onExit();
  CHECK(renderer.orientation == GfxRenderer::Portrait);
  a.~RsvpReaderActivity();
  // Failed submission must not commit the staged next word or return its offset.
  ui = {};
  active = new (memory) RsvpReaderActivity(renderer, input, std::shared_ptr<Epub>(&epub), {}, false);
  auto& b = *active;
  b.onEnter();
  b.render(RenderLock{});
  b.render(RenderLock{});
  CHECK(b.history.current()->startOffset == 0);
  submitSuccess = false;
  input.release(MappedInputManager::Button::Right);
  b.loop();
  b.render(RenderLock{});
  CHECK(b.history.current()->startOffset == 0 && b.stagedAvailable && b.source.cursor == 2);
  CHECK(b.state.load() == RsvpReaderActivity::State::Paused);
  input.release(MappedInputManager::Button::Back);
  b.loop();
  CHECK(b.finished && b.result.visibleTextOffset == 0);
  b.onExit();
  b.~RsvpReaderActivity();
  return 0;
}

extern "C" int releaseMain() {
  settings = {};
  for (uint16_t speed : RsvpCadence::SPEEDS) {
    const auto before = settings.writes;
    const bool changed = settings.rsvpWpm != speed;
    CHECK(settings.saveRsvpSpeed(speed));
    CHECK(settings.persistedWpm == speed && settings.writes == before + changed);
    CHECK(settings.saveRsvpSpeed(speed) && settings.writes == before + changed);
  }
  const auto writes = settings.writes;
  CHECK(!settings.saveRsvpSpeed(51) && settings.writes == writes);
  settings.writeSuccess = false;
  CHECK(!settings.saveRsvpSpeed(75) && settings.rsvpWpm == 125 && settings.persistedWpm == 125);
  settings.writeSuccess = true;
  CHECK(settings.saveRsvpSpeed(75) && settings.persistedWpm == 75);

  GfxRenderer renderer;
  MappedInputManager input;
  Epub epub;
  alignas(RsvpReaderActivity) static unsigned char memory[sizeof(RsvpReaderActivity)];
  submitSuccess = true;
  submitHook = nullptr;
  for (unsigned session = 0; session < 40; ++session) {
    settings.rsvpSmartTiming = session % 2;
    active = new (memory) RsvpReaderActivity(renderer, input, std::shared_ptr<Epub>(&epub), {}, false);
    auto& a = *active;
    a.onEnter();
    a.render(RenderLock{});
    a.render(RenderLock{});
    CHECK(a.cadence.wpm() == 75 && a.cadence.smartTimingEnabled() == bool(session % 2));
    const auto saves = settings.writes;
    const auto calls = a.source.nextCalls;
    for (unsigned cycle = 0; cycle < 20; ++cycle) {
      input.release(MappedInputManager::Button::Confirm);
      a.loop();
      const auto start = now;
      a.render(RenderLock{});
      CHECK(a.history.current()->startOffset == 0 && a.source.nextCalls == calls);
      CHECK(a.state.load() == RsvpReaderActivity::State::Playing);
      CHECK(!a.cadence.due(start + 799999) && a.cadence.due(start + 800000));
      settings.rsvpSmartTiming ^= 1;
      a.loop();
      CHECK(a.history.current()->startOffset == 0 && a.source.nextCalls == calls);
      input.release(MappedInputManager::Button::Confirm);
      a.loop();
      a.render(RenderLock{});
      CHECK(a.state.load() == RsvpReaderActivity::State::Paused && a.contextOffset == 0);
    }
    input.release(MappedInputManager::Button::Down);
    a.loop();
    a.render(RenderLock{});
    CHECK(a.cadence.wpm() == 100 && settings.writes == saves);
    input.release(MappedInputManager::Button::Up);
    a.loop();
    a.render(RenderLock{});
    a.onExit();
    CHECK(settings.writes == saves && settings.rsvpWpm == 75);
    a.~RsvpReaderActivity();
  }
  // Traverse an empty middle chapter and retain exactly the four boundary words.
  active = new (memory) RsvpReaderActivity(renderer, input, std::shared_ptr<Epub>(&epub), {}, false);
  auto& a = *active;
  a.source.emptyMiddle = true;
  a.source.finalChapter = 2;
  a.onEnter();
  a.render(RenderLock{});
  a.render(RenderLock{});
  input.release(MappedInputManager::Button::Right);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->spineIndex == 0 && a.history.current()->startOffset == 4);
  input.release(MappedInputManager::Button::Right);
  a.loop();
  for (unsigned tick = 0; tick < 8; ++tick) {
    a.render(RenderLock{});
    a.loop();
  }
  CHECK(a.history.current()->spineIndex == 2 && a.history.current()->startOffset == 0 && a.source.cursor == 3);
  input.release(MappedInputManager::Button::Left);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->spineIndex == 0 && a.source.cursor == 3);
  input.release(MappedInputManager::Button::Right);
  a.loop();
  a.render(RenderLock{});
  CHECK(a.history.current()->spineIndex == 2 && a.source.cursor == 3);
  input.release(MappedInputManager::Button::Down);
  a.loop();
  a.render(RenderLock{});
  const auto saves = settings.writes;
  a.onExit();
  CHECK(settings.persistedWpm == 100 && settings.writes == saves + 1);
  a.~RsvpReaderActivity();
  // An entirely empty EPUB reaches a bounded paused EOF without a selected token.
  active = new (memory) RsvpReaderActivity(renderer, input, std::shared_ptr<Epub>(&epub), {}, false);
  auto& b = *active;
  b.source.allEmpty = true;
  b.onEnter();
  for (unsigned tick = 0; tick < 12; ++tick) {
    b.render(RenderLock{});
    b.loop();
  }
  CHECK(b.endReached && !b.history.current() && b.state.load() == RsvpReaderActivity::State::Paused);
  const auto eofSubmissions = submissions;
  for (unsigned tick = 0; tick < 20; ++tick) {
    b.loop();
    b.render(RenderLock{});
  }
  CHECK(submissions == eofSubmissions);
  b.onExit();
  b.~RsvpReaderActivity();
  return 0;
}
