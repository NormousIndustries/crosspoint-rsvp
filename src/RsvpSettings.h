#pragma once

#include <ArduinoJson.h>

#include <cstdint>

namespace rsvp {

inline void loadSettings(JsonVariantConst doc, uint8_t& wpm, uint8_t& smartTiming) {
  const auto speed = doc["rsvpWpm"];
  const auto timing = doc["rsvpSmartTiming"];
  wpm = 100;
  if (speed.is<unsigned>() && (speed.as<unsigned>() == 50 || speed.as<unsigned>() == 75 ||
                               speed.as<unsigned>() == 100 || speed.as<unsigned>() == 125)) {
    wpm = speed.as<uint8_t>();
  }
  smartTiming = timing.is<unsigned>() && timing.as<unsigned>() <= 1 ? timing.as<uint8_t>() : 1;
}

}  // namespace rsvp
