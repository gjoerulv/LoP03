#pragma once

#include "raylib.h"

// Named typography roles, spacing, and shared UI colors (M12-c; M22 palette;
// M46 facelift). States use these instead of ad-hoc numeric sizes/colors so
// conventions stay in one place. Documented in docs/ui_style_guide.md — update
// both together.

namespace cd::ui::style {

// --- Text roles ---
inline constexpr int kFontTitleHero = 22;    // title-screen game name only
inline constexpr int kFontScreenTitle = 16;  // screen headings
inline constexpr int kFontHeading = 14;      // panel headings (pause boxes)
inline constexpr int kFontMenuLarge = 14;    // title-screen menu
inline constexpr int kFontMenu = 11;         // standard menus and lists
inline constexpr int kFontBody = 10;         // messages, prompts, wrapped text
inline constexpr int kFontSmall = 8;         // HUD/captions — legibility floor at 1x

// --- Spacing ---
inline constexpr int kSafeMargin = 4;   // outer safe-area margin
inline constexpr int kPad = 8;          // default panel padding
inline constexpr int kPadSmall = 4;
inline constexpr int kFooterHeight = 16;  // reserved control-hint strip

// --- Color palette (M22 accessor; M46 storybook re-value) ---
// Two palettes: the standard "humorous 8-bit-plus" storybook table and a
// high-contrast table (pure white text, brighter borders and accents, darker
// fills). palette() reads the active table; the ONLY writer is the
// settings-apply path (Application / SettingsState) via setHighContrast — a
// deliberate, documented exception to the no-global-mutable-state rule
// (single-threaded game loop, plain data, one writer).
//
// Color is never the only information channel: every role below is paired
// with a shape/marker/text convention in ui/UiDraw (selection slabs +
// chevrons, danger notch shapes, meter labels, disabled reasons).
struct Palette {
    // Text roles (pre-M46 set, re-valued to the warm storybook ramp).
    Color text;        // primary text — warm cream, not pure white
    Color textDim;     // secondary text
    Color textHint;    // footer hints, captions
    Color disabled;    // disabled rows (always paired with a reason/shape)
    Color cursor;      // selection accent (gold)
    Color success;     // success/heal text
    Color dangerText;  // danger text (readable on dark fills)
    Color gold;        // gold/reward values

    // Surfaces (M46).
    Color canvas;       // screen background behind everything
    Color ink;          // outline / deepest ink / hard shadows
    Color panel;        // standard panel fill
    Color panelRaised;  // raised & modal panel fill
    Color panelInset;   // inset wells (list panels, meter tracks' parent)
    Color borderDark;   // bottom/right depth edge
    Color borderMid;    // middle border line
    Color borderLight;  // top/left highlight edge

    // Accents (M46).
    Color crystal;  // crystal cyan
    Color magic;    // magic violet
    Color danger;   // danger coral (fills, borders)

    // Meters (M46). HP/MP stay text-labelled wherever they gate decisions.
    Color hpFill;
    Color mpFill;
    Color meterBack;

    // Structures (M46).
    Color rowFill;     // selected-row slab fill
    Color rowBorder;   // selected-row slab border
    Color footerFill;  // footer hint strip fill
    Color modalDim;    // full-screen dim veil under modals (with alpha)
    Color keycapFill;  // compact keycap fill
    Color chipFill;    // value chip / badge fill
};

const Palette& palette();
void setHighContrast(bool on);
bool highContrastActive();

// --- Legacy color constants (standard palette values) ---
// Prefer palette() in new/updated code; these remain for compile-time
// contexts and match the standard palette's text roles exactly.
inline constexpr Color kText{243, 234, 216, 255};      // warm cream (M46)
inline constexpr Color kTextDim{196, 188, 172, 255};
inline constexpr Color kTextHint{160, 153, 144, 255};
inline constexpr Color kDisabled{122, 116, 134, 255};
inline constexpr Color kCursor{230, 189, 106, 255};    // gold accent
inline constexpr Color kSuccess{108, 203, 114, 255};
inline constexpr Color kDangerText{232, 133, 133, 255};
inline constexpr Color kGold{230, 189, 106, 255};

}  // namespace cd::ui::style
