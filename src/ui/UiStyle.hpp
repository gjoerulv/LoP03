#pragma once

#include "raylib.h"

// Named typography roles, spacing, and shared UI colors (M12-c; M22 palette).
// States use these instead of ad-hoc numeric sizes/colors so conventions stay
// in one place. Documented in docs/ui_style_guide.md — update both together.

namespace cd::ui::style {

// --- Text roles (raylib default font sizes for now) ---
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

// --- Color palette (M22, owner-approved) ---
// Two palettes: the standard one keeps the exact pre-M22 colors; the
// high-contrast one uses pure white/black pairs and brighter accents.
// palette() reads the active table; the ONLY writer is the settings-apply
// path (Application / SettingsState) via setHighContrast — a deliberate,
// documented exception to the no-global-mutable-state rule (single-threaded
// game loop, plain data, one writer).
struct Palette {
    Color text;
    Color textDim;
    Color textHint;
    Color disabled;
    Color cursor;
    Color success;
    Color dangerText;
    Color gold;
};

const Palette& palette();
void setHighContrast(bool on);
bool highContrastActive();

// --- Legacy color constants (standard palette values) ---
// Prefer palette() in new/updated code; these remain for compile-time
// contexts and match the standard palette exactly.
inline constexpr Color kText{245, 245, 245, 255};        // ~RAYWHITE
inline constexpr Color kTextDim{170, 170, 190, 255};
inline constexpr Color kTextHint{150, 150, 170, 255};
inline constexpr Color kDisabled{90, 90, 110, 255};
inline constexpr Color kCursor{240, 220, 120, 255};
inline constexpr Color kSuccess{170, 220, 170, 255};
inline constexpr Color kDangerText{225, 150, 150, 255};
inline constexpr Color kGold{230, 220, 150, 255};

}  // namespace cd::ui::style
