#include <new>

#include "RsvpCadence.h"
#include "RsvpSettings.h"

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
  for (size_t i = 0; i < size; ++i)
    if (x[i] != y[i]) return x[i] - y[i];
  return 0;
}
extern "C" size_t strlen(const char* s) {
  size_t n = 0;
  while (s[n]) ++n;
  return n;
}
extern "C" int strcmp(const char* a, const char* b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}
extern "C" void* malloc(size_t) { return nullptr; }
extern "C" void* realloc(void*, size_t) { return nullptr; }
extern "C" void free(void*) {}
void operator delete(void*) noexcept {}
void operator delete(void*, size_t) noexcept {}
#endif

#define CHECK(condition)               \
  do {                                 \
    if (!(condition)) return __LINE__; \
  } while (false)

class TestAllocator final : public ArduinoJson::Allocator {
 public:
  alignas(8) unsigned char bytes[65536]{};
  size_t used = 0;
  unsigned live = 0;
  void* allocate(size_t size) override {
    size = (size + 7) & ~size_t(7);
    if (used + size + 8 > sizeof(bytes)) return nullptr;
    auto* block = bytes + used;
    memcpy(block, &size, sizeof(size));
    used += size + 8;
    ++live;
    return block + 8;
  }
  void deallocate(void* ptr) override {
    if (ptr) --live;
  }
  void* reallocate(void* ptr, size_t size) override {
    if (!ptr) return allocate(size);
    size_t oldSize;
    memcpy(&oldSize, static_cast<unsigned char*>(ptr) - 8, sizeof(oldSize));
    if (size <= oldSize) return ptr;
    auto* result = allocate(size);
    if (!result) return nullptr;
    memcpy(result, ptr, oldSize);
    deallocate(ptr);
    return result;
  }
} allocator;

extern "C" int testMain() {
  uint8_t speed = 75, smart = 0;
  {
    JsonDocument doc(&allocator);
    rsvp::loadSettings(doc.as<JsonVariantConst>(), speed, smart);
    CHECK(speed == 100 && smart == 1);
  }
  CHECK(allocator.live == 0);
  allocator.used = 0;
  // Round-trip real ArduinoJson documents at every supported selection.
  for (uint16_t value : RsvpCadence::SPEEDS) {
    for (unsigned enabled = 0; enabled < 2; ++enabled) {
      {
        JsonDocument saved(&allocator), loaded(&allocator);
        saved["rsvpWpm"] = value;
        saved["rsvpSmartTiming"] = enabled;
        saved["fontSize"] = 14;
        char json[128];
        const size_t length = serializeJson(saved, json, sizeof(json));
        CHECK(length < sizeof(json) && !deserializeJson(loaded, json));
        rsvp::loadSettings(loaded.as<JsonVariantConst>(), speed, smart);
        CHECK(speed == value && smart == enabled && loaded["fontSize"].as<unsigned>() == 14);
        CHECK(loaded["spineIndex"].isNull() && loaded["visibleTextOffset"].isNull());
        RsvpCadence cadence;
        cadence.setWpm(speed);
        cadence.setSmartTiming(smart != 0);
        CHECK(cadence.intervalUs() == 60000000LL / value);
        for (uint8_t t = 0; t < static_cast<uint8_t>(rsvp::Timing::Count); ++t) {
          const auto timing = static_cast<rsvp::Timing>(t);
          const int64_t interval = cadence.intervalUs() + (enabled ? rsvp::adjustmentMs(timing) * 1000LL : 0);
          cadence.completed(1000, 100100, timing);
          CHECK(!cadence.due(1000 + interval - 1) && cadence.due(1000 + interval));
          cadence.rebase(2000);
          CHECK(!cadence.due(2000 + interval - 1) && cadence.due(2000 + interval));
        }
        if (!enabled) CHECK(cadence.additionalUs == 0);
      }
      CHECK(allocator.live == 0);
      allocator.used = 0;
    }
  }
  static constexpr const char* CORRUPT[] = {"null", "true", "false",      "\"75\"", "-1", "0", "51",
                                            "126",  "306",  "4294967296", "75.5",   "[]", "{}"};
  for (const auto* value : CORRUPT) {
    {
      JsonDocument doc(&allocator);
      CHECK(!deserializeJson(doc, value));
      JsonDocument settings(&allocator);
      settings["rsvpWpm"] = doc.as<JsonVariantConst>();
      settings["rsvpSmartTiming"] = doc.as<JsonVariantConst>();
      rsvp::loadSettings(settings.as<JsonVariantConst>(), speed, smart);
      CHECK(speed == 100 && smart == (strcmp(value, "0") == 0 ? 0 : 1));
    }
    CHECK(allocator.live == 0);
    allocator.used = 0;
  }
  {
    JsonDocument doc(&allocator);
    CHECK(deserializeJson(doc, "{bad"));
  }
  CHECK(allocator.live == 0);
  RsvpCadence cadence;
  cadence.setWpm(65535);
  CHECK(cadence.wpm() == 100 && cadence.smartTimingEnabled());
  cadence.completed(0, 100000, rsvp::Timing::Paragraph);
  cadence.setSmartTiming(false);
  cadence.rebase(100000);
  CHECK(!cadence.due(699999) && cadence.due(700000));
  cadence.setSmartTiming(true);
  cadence.rebase(100000);
  CHECK(!cadence.due(1199999) && cadence.due(1200000));
  return 0;
}

#ifndef RSVP_FREESTANDING
int main() { return testMain(); }
#endif
