#include "ui/UiStyle.hpp"

namespace cd::ui::style {

namespace {

// Standard palette: the M46 storybook table. Text roles match the legacy
// constants exactly (test-pinned); surfaces follow the approved foundation
// (ink #0E0C14, panels #171522/#252235, borders #383349/#736C7F, accents
// #4ED8D3/#8F63E8/#D95757/#6CCB72/#5D8BE8, gold #E6BD6A).
constexpr Palette kStandard{
    kText, kTextDim, kTextHint, kDisabled, kCursor, kSuccess, kDangerText, kGold,
    Color{16, 14, 24, 255},     // canvas
    Color{14, 12, 20, 255},     // ink
    Color{23, 21, 34, 255},     // panel
    Color{37, 34, 53, 255},     // panelRaised
    Color{17, 15, 26, 255},     // panelInset
    Color{56, 51, 73, 255},     // borderDark
    Color{115, 108, 127, 255},  // borderMid
    Color{168, 160, 180, 255},  // borderLight
    Color{78, 216, 211, 255},   // crystal
    Color{143, 99, 232, 255},   // magic
    Color{217, 87, 87, 255},    // danger
    Color{108, 203, 114, 255},  // hpFill
    Color{93, 139, 232, 255},   // mpFill
    Color{27, 24, 39, 255},     // meterBack
    Color{54, 47, 82, 255},     // rowFill
    Color{166, 138, 82, 255},   // rowBorder
    Color{20, 18, 30, 255},     // footerFill
    Color{10, 8, 16, 170},      // modalDim
    Color{46, 42, 63, 255},     // keycapFill
    Color{30, 27, 44, 255},     // chipFill
};

// High contrast: pure white primary text, every text role at least as bright
// as its standard counterpart, darker fills with brighter borders so shape
// separation *increases* — the mode is a contrast boost, never a recolor that
// loses the marker/shape double signals.
constexpr Palette kHighContrast{
    Color{255, 255, 255, 255},  // text
    Color{228, 228, 236, 255},  // textDim
    Color{212, 212, 224, 255},  // textHint
    Color{156, 152, 172, 255},  // disabled (still visibly "off", but readable)
    Color{255, 216, 92, 255},   // cursor
    Color{140, 255, 146, 255},  // success
    Color{255, 132, 132, 255},  // dangerText
    Color{255, 216, 110, 255},  // gold
    Color{8, 8, 12, 255},       // canvas
    Color{0, 0, 0, 255},        // ink
    Color{16, 16, 24, 255},     // panel
    Color{30, 30, 44, 255},     // panelRaised
    Color{8, 8, 14, 255},       // panelInset
    Color{96, 96, 118, 255},    // borderDark
    Color{178, 178, 198, 255},  // borderMid
    Color{240, 240, 250, 255},  // borderLight
    Color{114, 240, 235, 255},  // crystal
    Color{186, 146, 255, 255},  // magic
    Color{255, 96, 96, 255},    // danger
    Color{124, 255, 132, 255},  // hpFill
    Color{132, 172, 255, 255},  // mpFill
    Color{16, 16, 26, 255},     // meterBack
    Color{66, 58, 104, 255},    // rowFill
    Color{255, 216, 92, 255},   // rowBorder
    Color{8, 8, 14, 255},       // footerFill
    Color{0, 0, 0, 190},        // modalDim
    Color{58, 54, 82, 255},     // keycapFill
    Color{34, 32, 52, 255},     // chipFill
};

bool gHighContrast = false;

}  // namespace

const Palette& palette() { return gHighContrast ? kHighContrast : kStandard; }

void setHighContrast(bool on) { gHighContrast = on; }

bool highContrastActive() { return gHighContrast; }

}  // namespace cd::ui::style
