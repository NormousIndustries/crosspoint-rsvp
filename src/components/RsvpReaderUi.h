#pragma once

#include <RsvpContext.h>
#include <RsvpTypography.h>

#include "themes/BaseTheme.h"

struct RsvpReaderLayout {
  Rect safe;
  Rect word;
  Rect context;
  int contextFont = 0;
  int contextLineHeight = 0;
  int focusX = 0;
  int baseline = 0;
  int upperTip = 0;
  int lowerTip = 0;
  int controlsY = 0;
  int rowHeight = 0;
};
