#pragma once

#include <HalDisplay.h>

class RsvpDisplayBackend {
 public:
  struct Completion {
    bool success;
    uint32_t serviceUs;
  };
  Completion submit();

 private:
  HalDisplay::DisplayMeasurement measurement;
};
