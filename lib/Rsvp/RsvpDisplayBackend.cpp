#include "RsvpDisplayBackend.h"

#include <Logging.h>

RsvpDisplayBackend::Completion RsvpDisplayBackend::submit() {
  // Blocking completion owns the single framebuffer until the HAL returns.
  display.displayBufferMeasuredFast(measurement);
  const bool success = !measurement.timeout && !measurement.busyNotObserved && measurement.refreshCount != 0;
  if (!success) {
    LOG_ERR("RSVP1", "FAST refresh failed: timeout=%u no_busy=%u", measurement.timeout, measurement.busyNotObserved);
  }
  return {success, measurement.totalUs};
}
