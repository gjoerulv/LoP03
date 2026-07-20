#include "ui/UiStyle.hpp"

namespace cd::ui::style {

namespace {

// Standard palette: byte-identical to the pre-M22 constants.
constexpr Palette kStandard{
    kText, kTextDim, kTextHint, kDisabled, kCursor, kSuccess, kDangerText, kGold,
};

// High contrast: pure white on the near-black backgrounds, no dim grays
// below the documented contrast floor, brighter accents.
constexpr Palette kHighContrast{
    Color{255, 255, 255, 255},  // text
    Color{225, 225, 235, 255},  // textDim
    Color{210, 210, 225, 255},  // textHint
    Color{140, 140, 160, 255},  // disabled (still visibly "off", but readable)
    Color{255, 240, 90, 255},   // cursor
    Color{140, 255, 140, 255},  // success
    Color{255, 120, 120, 255},  // dangerText
    Color{255, 235, 120, 255},  // gold
};

bool gHighContrast = false;

}  // namespace

const Palette& palette() { return gHighContrast ? kHighContrast : kStandard; }

void setHighContrast(bool on) { gHighContrast = on; }

bool highContrastActive() { return gHighContrast; }

}  // namespace cd::ui::style
