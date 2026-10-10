#include "X3WindowTestActivity.h"

#include <BoardConfig.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <XteinkDetect.h>
#include <esp_heap_caps.h>

#include <algorithm>

#include "components/UITheme.h"
#include "components/UIScale.h"

void X3WindowTestActivity::onEnter() {
  Activity::onEnter();
  RenderLock lock;
  previousOrientation = renderer.getOrientation();
  previousRenderMode = renderer.getRenderMode();
  if (!gpio.deviceIsX3() || display.isInverted()) {
    LOG_ERR("X3WIN", "Requires X3 and non-inverted B/W output; disable dark mode before entry");
    finish();
    return;
  }
  const auto& probe = freeink::getXteinkDisplayProbeDiag();
  LOG_INF("X3WIN", "board=%s controller=%s variant=%u VER=%02X,%02X,%02X probe_valid=%u probe_timeout=%u",
          BoardConfig::ACTIVE.name, display.getController() == HalDisplay::Controller::UC8253 ? "UC8253" : "UC8279",
          BoardConfig::ACTIVE.displayControllerVariant, probe.ver[0], probe.ver[1], probe.ver[2], probe.valid,
          probe.busyTimedOut);
  if (!probe.valid || probe.busyTimedOut || (probe.ver[2] != 0x66 && probe.ver[2] != 0xFF)) {
    LOG_ERR("X3WIN", "Inconclusive controller probe; manual experiment unavailable");
    finish();
    return;
  }
  renderer.setOrientation(GfxRenderer::LandscapeCounterClockwise);
  renderer.setRenderMode(GfxRenderer::BW);
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  safeRect = Rect(left, top, renderer.getScreenWidth() - left - right, renderer.getScreenHeight() - top - bottom);
  const auto& metrics = UITheme::getInstance().getMetrics();
  constexpr int TARGET_WIDTH = 704, TARGET_HEIGHT = 160;
  const int width = std::min(TARGET_WIDTH, safeRect.width - 2 * metrics.contentSidePadding);
  const int height = std::min(TARGET_HEIGHT, safeRect.height - 2 * (metrics.headerHeight + metrics.topPadding));
  patternRect = Rect(safeRect.x + (safeRect.width - width) / 2, safeRect.y + (safeRect.height - height) / 2, width, height);
  updateGeometry();
  LOG_INF("X3WIN", "manual-only activity_bytes=%u; Confirm=one transition Left=cleanup Right=geometry Back=exit",
          static_cast<unsigned>(sizeof(*this)));
  requestUpdate();
}

void X3WindowTestActivity::updateGeometry() {
  screenRect = patternRect;
  if (geometry == Geometry::FullWidth) screenRect = Rect(0, patternRect.y, renderer.getScreenWidth(), patternRect.height);
  if (geometry == Geometry::FullFrame) screenRect = Rect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  if (!renderer.getAlignedDisplayRect(screenRect.x, screenRect.y, screenRect.width, screenRect.height, memoryRect)) {
    LOG_ERR("X3WIN", "Invalid runtime geometry");
    baselineValid = false;
    job = Job::None;
  }
}

void X3WindowTestActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) pendingControls |= EXIT;
  if (mappedInput.wasReleased(Button::Confirm)) pendingControls |= STEP;
  if (mappedInput.wasReleased(Button::Left)) pendingControls |= CLEAN;
  if (mappedInput.wasReleased(Button::Right)) pendingControls |= GEOMETRY;
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  printReport();
  const uint8_t controls = pendingControls;
  pendingControls = 0;
  if (controls & EXIT) { job = Job::None; finish(); return; }
  if (controls & (CLEAN | GEOMETRY)) {
    if (controls & GEOMETRY) geometry = static_cast<Geometry>((static_cast<unsigned>(geometry) + 1) % 3);
    patternB = false;
    baselineValid = false;
    job = Job::Baseline;
    updateGeometry();
  } else if ((controls & STEP) && job == Job::None) {
    if (!baselineValid) {
      LOG_ERR("X3WIN", "STOP: baseline invalid; Left requires full cleanup before another experiment");
      return;
    }
    job = Job::Transition;
  }
  if (job != Job::None) requestUpdate();
}

void X3WindowTestActivity::drawPattern() {
  renderer.fillRect(patternRect.x, patternRect.y, patternRect.width, patternRect.height, false);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int padding = metrics.contentSidePadding;
  const int halfWidth = patternRect.width / 2;
  Rect textRect(patternRect.x + (patternB ? halfWidth : 0), patternRect.y, halfWidth, patternRect.height);
  const int y = patternB ? patternRect.y + patternRect.height - padding - renderer.getLineHeight(uiScaleSpec().titleFontId)
                         : patternRect.y + padding;
  UITheme::drawCenteredText(renderer, textRect, uiScaleSpec().titleFontId, y,
                            patternB ? tr(STR_X3_WINDOW_PATTERN_B) : tr(STR_X3_WINDOW_PATTERN_A));
}

void X3WindowTestActivity::drawBaseline() {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const char* mode = tr(STR_X3_WINDOW_REGION);
  if (geometry == Geometry::FullFrame) mode = tr(STR_X3_WINDOW_FULL_FRAME);
  if (geometry == Geometry::FullWidth) mode = tr(STR_X3_WINDOW_FULL_WIDTH);
  GUI.drawHeader(renderer, Rect(safeRect.x, safeRect.y, safeRect.width - metrics.buttonHintsHeight, metrics.headerHeight),
                 tr(STR_X3_WINDOW_TEST));
  UITheme::drawCenteredText(renderer, safeRect, uiScaleSpec().smallFontId, safeRect.y + metrics.headerHeight + metrics.topPadding,
                            mode);
  UITheme::drawCenteredText(renderer, safeRect, uiScaleSpec().smallFontId,
                            patternRect.y + patternRect.height + metrics.verticalSpacing, tr(STR_X3_WINDOW_EXTERIOR));
  GUI.drawButtonHints(renderer, tr(STR_BACK), tr(STR_X3_WINDOW_STEP), tr(STR_X3_WINDOW_CLEAN), tr(STR_X3_WINDOW_GEOMETRY));
  drawPattern();
}

void X3WindowTestActivity::render(RenderLock&& lock) {
  const Job current = job;
  if (current == Job::None) return;
  job = Job::None;
  lastWasBaseline = current == Job::Baseline;
  renderer.setRenderMode(GfxRenderer::BW);
  if (lastWasBaseline) {
    drawBaseline();
    display.displayBufferMeasured(HalDisplay::FULL_REFRESH, measurement);
    ++cleanupCount;
    baselineValid = measurement.refreshCount && !measurement.timeout && !measurement.busyNotObserved;
    result = baselineValid ? HalDisplay::WindowFastResult::Success : HalDisplay::WindowFastResult::InvalidState;
  } else {
    patternB = !patternB;
    drawPattern();
    if (geometry == Geometry::FullFrame) {
      display.displayBufferMeasuredFast(measurement);
      result = measurement.effectiveFast && measurement.refreshCount == 1 && !measurement.timeout && !measurement.busyNotObserved
                   ? HalDisplay::WindowFastResult::Success : HalDisplay::WindowFastResult::InvalidState;
    } else {
      result = display.displayWindowFastExperimental(memoryRect, measurement);
    }
    baselineValid = result == HalDisplay::WindowFastResult::Success;
    if (baselineValid) ++transitions;
  }
  constexpr uint32_t CAPS = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  freeHeap = heap_caps_get_free_size(CAPS);
  minimumHeap = heap_caps_get_minimum_free_size(CAPS);
  largestBlock = heap_caps_get_largest_free_block(CAPS);
  reportPending = true;
}

void X3WindowTestActivity::printReport() {
  if (!reportPending) return;
  reportPending = false;
  const auto& probe = freeink::getXteinkDisplayProbeDiag();
  LOG_INF("X3WIN", "IDENTITY controller=%s variant=%u VER=%02X,%02X,%02X",
          display.getController() == HalDisplay::Controller::UC8253 ? "UC8253" : "UC8279",
          BoardConfig::ACTIVE.displayControllerVariant, probe.ver[0], probe.ver[1], probe.ver[2]);
  LOG_INF("X3WIN", "RESULT job=%s geometry=%u status=%u baseline_valid=%u transitions=%u cleanup=%u",
          lastWasBaseline ? "full-clean" : "manual-step", static_cast<unsigned>(geometry), static_cast<unsigned>(result),
          baselineValid, transitions, cleanupCount);
  const HalDisplay::DisplayMemoryRect fullRect{0, 0, renderer.getDisplayWidth(), renderer.getDisplayHeight()};
  const auto& actualRect = lastWasBaseline ? fullRect : memoryRect;
  LOG_INF("X3WIN", "RECT screen=%d,%d,%d,%d aligned_memory=%u,%u,%u,%u gates=%u..%u stride=%u",
          lastWasBaseline ? 0 : screenRect.x, lastWasBaseline ? 0 : screenRect.y,
          lastWasBaseline ? renderer.getScreenWidth() : screenRect.width,
          lastWasBaseline ? renderer.getScreenHeight() : screenRect.height, actualRect.x, actualRect.y, actualRect.width,
          actualRect.height, renderer.getDisplayHeight() - (actualRect.y + actualRect.height),
          renderer.getDisplayHeight() - 1 - actualRect.y, renderer.getDisplayWidthBytes());
  LOG_INF("X3WIN", "PANEL fast=%u refreshes=%u NEW_bytes=%lu OLD_bytes=%lu NEW_us=%lu OLD_us=%lu wait_us=%lu setup_other_us=%lu total_us=%lu",
          measurement.effectiveFast, measurement.refreshCount, static_cast<unsigned long>(measurement.newBytes),
          static_cast<unsigned long>(measurement.oldBytes), static_cast<unsigned long>(measurement.newUs),
          static_cast<unsigned long>(measurement.oldUs), static_cast<unsigned long>(measurement.waitUs),
          static_cast<unsigned long>(measurement.totalUs - measurement.newUs - measurement.oldUs - measurement.waitUs),
          static_cast<unsigned long>(measurement.totalUs));
  LOG_INF("X3WIN", "WAVE call_us=%lld DRF_us=%lld BUSY_observed_us=%lld BUSY_complete_us=%lld DRF_to_complete_us=%lld LOW_to_complete_us=%lld timeout=%u assertion_missing=%u",
          measurement.callStartUs, measurement.drfUs, measurement.busyObservedUs, measurement.busyCompleteUs,
          measurement.busyCompleteUs ? measurement.busyCompleteUs - measurement.drfUs : 0,
          measurement.busyObservedUs ? measurement.busyCompleteUs - measurement.busyObservedUs : 0,
          measurement.timeout, measurement.busyNotObserved);
  LOG_INF("X3WIN", "HEAP free=%lu boot_min=%lu largest=%lu (internal 8-bit)", static_cast<unsigned long>(freeHeap),
          static_cast<unsigned long>(minimumHeap), static_cast<unsigned long>(largestBlock));
  if (!baselineValid) LOG_ERR("X3WIN", "STOP: inspect panel; no further experimental refresh until explicit full cleanup");
}

void X3WindowTestActivity::onExit() {
  job = Job::None;
  printReport();
  renderer.setOrientation(previousOrientation);
  renderer.setRenderMode(previousRenderMode);
  Activity::onExit();
}
