#include "RsvpReaderUi.h"

#include <GfxRenderer.h>
#include <GlyphBitmap.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "UITheme.h"
#include "UIScale.h"

namespace {
constexpr int MARKER_HALF_WIDTH = 5;
constexpr int MARKER_HEIGHT = 6;
constexpr int MARKER_GAP = 18;
constexpr int WORD_TOP_EXTENT = 158;  // Includes raised decomposed Latin marks.
constexpr int WORD_BOTTOM_EXTENT = 36;

struct GlyphContext {
  const GfxRenderer& renderer;
  const RsvpReaderLayout& layout;
  int x;
};

void drawGlyph(void* ctx, const EpdGlyph& glyph, const uint8_t* bitmap, int x, int y) {
  const auto& draw = *static_cast<GlyphContext*>(ctx);
  const glyphBitmap::Frame frame{draw.x + x + glyph.left, draw.layout.baseline + y - glyph.top, 1, 0, 0, 1};
  draw.renderer.drawGlyphBitmap(bitmap, glyph.width, glyph.height, frame, false, GfxRenderer::BW, true);
}

void hint(const GfxRenderer& renderer, Rect cell, int fontId, const char* text, bool rightAligned = false) {
  // Bounded clip also protects unusually long translations; English labels fit intact.
  const auto clip = renderer.getClipRect();
  renderer.setClipRect(cell.x, cell.y, cell.width, cell.height);
  if (rightAligned) {
    renderer.drawText(fontId, cell.x + cell.width - renderer.getTextWidth(fontId, text), cell.y, text);
  } else {
    UITheme::drawCenteredText(renderer, cell, fontId, cell.y, text);
  }
  renderer.setClipRect(clip[0], clip[1], clip[2], clip[3]);
}
}  // namespace

RsvpReaderLayout UITheme::rsvpLayout(const GfxRenderer& renderer) const {
  RsvpReaderLayout layout;
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const int margin = std::max(24, getMetrics().contentSidePadding);
  layout.safe = Rect(left + margin, top + margin, renderer.getScreenWidth() - left - right - 2 * margin,
                     renderer.getScreenHeight() - top - bottom - 2 * margin);
  layout.focusX = layout.safe.x + layout.safe.width * 38 / 100;
  layout.baseline = top + (renderer.getScreenHeight() - top - bottom) / 2 + 47;
  layout.word = Rect(layout.safe.x, layout.baseline - WORD_TOP_EXTENT, layout.safe.width,
                     WORD_TOP_EXTENT + WORD_BOTTOM_EXTENT);
  layout.upperTip = layout.word.y - MARKER_GAP;
  layout.lowerTip = layout.word.y + layout.word.height + MARKER_GAP;
  layout.rowHeight = renderer.getLineHeight(uiScaleSpec().smallFontId);
  layout.controlsY = layout.safe.y + layout.safe.height - 2 * layout.rowHeight;
  return layout;
}

void UITheme::drawRsvpWord(const GfxRenderer& renderer, const RsvpReaderLayout& layout, const char* text,
                           const rsvp::Placement& placement) const {
  const auto clip = renderer.getClipRect();
  renderer.setClipRect(layout.word.x, layout.word.y, layout.word.width, layout.word.height);
  GlyphContext context{renderer, layout, placement.x};
  const auto orp = rsvp::findOrp(text);
  rsvp::walk(rsvp::font(placement.size), text, orp.byteOffset, &context, drawGlyph);
  renderer.setClipRect(clip[0], clip[1], clip[2], clip[3]);
  // Two filled triangles, with their single-pixel tips on the focal column.
  for (int row = 0; row < MARKER_HEIGHT; ++row) {
    const int halfWidth = row * MARKER_HALF_WIDTH / (MARKER_HEIGHT - 1);
    renderer.drawLine(layout.focusX - halfWidth, layout.upperTip - row, layout.focusX + halfWidth,
                      layout.upperTip - row);
    renderer.drawLine(layout.focusX - halfWidth, layout.lowerTip + row, layout.focusX + halfWidth,
                      layout.lowerTip + row);
  }
}

void UITheme::drawRsvpPaused(const GfxRenderer& renderer, const RsvpReaderLayout& layout, const char* status,
                             uint16_t wpm, bool oversized, const MappedInputManager& input,
                             const rsvp::Placement& placement) const {
  const int fontId = uiScaleSpec().smallFontId;
  char summary[96];
  snprintf(summary, sizeof(summary), tr(STR_RSVP_TEST_PLAY_STATUS), status, wpm);
  hint(renderer, Rect(layout.safe.x, layout.controlsY, layout.safe.width, layout.rowHeight), fontId,
       oversized ? tr(STR_RSVP_WORD_TOO_WIDE) : summary);
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const int edgeX = renderer.getScreenWidth() - right - 6;
  const int edgeHeight = renderer.getScreenHeight() - top - bottom;
  const int speedX = left + (renderer.getScreenWidth() - left - right) / 4;
  const int speedWidth = layout.safe.width / 3;
  hint(renderer, Rect(speedX - speedWidth / 2, layout.safe.y, speedWidth, layout.rowHeight), fontId,
       tr(STR_RSVP_FASTER));
  hint(renderer, Rect(speedX - speedWidth / 2, layout.safe.y + layout.safe.height - layout.rowHeight,
                       speedWidth, layout.rowHeight), fontId, tr(STR_RSVP_SLOWER));
  renderer.drawLine(speedX, top + 6, speedX, layout.safe.y - 4);
  renderer.drawLine(speedX, layout.safe.y + layout.safe.height + 4, speedX,
                    renderer.getScreenHeight() - bottom - 6);

  // In landscape CCW the front hardware order runs bottom-to-top. Remaps move hints with actions.
  const auto labels = input.mapFrontLabels(tr(STR_BACK), tr(STR_RSVP_PLAY_PAUSE), tr(STR_RSVP_PREVIOUS),
                                           tr(STR_RSVP_NEXT));
  const char* const ordered[] = {labels.btn4, labels.btn3, labels.btn2, labels.btn1};
  for (int i = 0; i < 4; ++i) {
    const int anchorY = top + edgeHeight * (i + 1) / 5;
    int labelY = anchorY - layout.rowHeight / 2;
    const int textRight = layout.safe.x + layout.safe.width;
    const int labelWidth = std::min(speedWidth, renderer.getTextWidth(fontId, ordered[i]));
    const auto& ink = placement.metrics;
    const bool overlaps = placement.x + ink.left < textRight &&
                          placement.x + ink.right > textRight - labelWidth &&
                          layout.baseline + ink.top < labelY + layout.rowHeight &&
                          layout.baseline + ink.bottom > labelY;
    // Only colliding middle hints move; the word's font, focus and baseline stay fixed.
    if (overlaps && i == 1) labelY = layout.word.y - layout.rowHeight - 8;
    if (overlaps && i == 2) labelY = layout.word.y + layout.word.height + 8;
    hint(renderer, Rect(textRight - speedWidth, labelY, speedWidth, layout.rowHeight), fontId, ordered[i], true);
    const int labelCenter = labelY + layout.rowHeight / 2;
    const int railX = textRight + 8;
    renderer.drawLine(textRight + 4, labelCenter, railX, labelCenter);
    renderer.drawLine(railX, labelCenter, railX, anchorY);
    renderer.drawLine(railX, anchorY, edgeX, anchorY);
    renderer.drawLine(edgeX, anchorY - 3, edgeX, anchorY + 3);
  }
}
