#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"
#include "ui/TextLayout.hpp"

// Shared raylib drawing helpers for the UI windows, plus the M46 procedural
// UI kit: hard-edged frames, plaques, chips, keycap footers, framed meters,
// selection slabs — all integer-aligned, palette-driven, drawn with
// primitives (no image assets). Original work; no copyrighted assets. Text
// placement is measured (M12): bounded helpers report overflow to the log
// once per site instead of silently clipping or spilling.

namespace cd {
class ResourceManager;
}

namespace cd::ui {

// Running count of text overflow/truncation events (every occurrence). The
// M23 capture tool asserts a zero delta per rendered scene; tests may too.
long overflowEvents();

// Panel that uses the nine-patch "ui.frame.default" texture when the catalog
// provides one (M15), and the M46 procedural Standard frame otherwise.
void drawFramedPanel(ResourceManager& resources, int x, int y, int w, int h, Color fill,
                     Color border);

// Full-screen scene background (M27): fills `fallback`, then draws the catalog
// texture `id` scaled to the virtual screen if present. A missing texture
// leaves only the fallback fill, so a state never regresses below its old
// solid ClearBackground.
void drawSceneBackground(ResourceManager& resources, const std::string& id, Color fallback,
                         int w, int h);

// Town-scoped variant (M32): draws "<id>.<town>" when that per-town texture
// exists (town >= 2), otherwise the base "<id>" — so service interiors vary per
// town with a safe fallback to their shared background.
void drawSceneBackground(ResourceManager& resources, const std::string& id, Color fallback,
                         int w, int h, int town);

// Installs the active UI fonts (M25): text is rendered with the base font
// whose native size is nearest the requested size, so pixel glyphs stay crisp
// (small=8, main=10, title=20). Any pointer may be null and any size with no
// installed font falls back to raylib's default font — a missing font asset
// never crashes. Non-owning: the fonts must outlive use, and setFonts must be
// called again after a ResourceManager reload invalidates them.
void setFonts(const Font* small, const Font* main, const Font* title);

// Left-aligned single line at (x, y) in the active font (DrawTextEx).
void drawText(const std::string& text, int x, int y, int fontSize, Color color);
void drawText(const char* text, int x, int y, int fontSize, Color color);

// Measurement against the active font (requires an initialized window).
int measureText(const std::string& text, int fontSize);
// The same measurement as an injectable TextMeasure for TextLayout calls.
const TextMeasure& raylibMeasure();

void drawTextCentered(const char* text, int centerX, int y, int fontSize, Color color);

// Right-aligned single line ending at rightX.
void drawTextRight(const std::string& text, int rightX, int y, int fontSize, Color color);

// Single line that must fit in maxWidth. If it does not, the overflow is
// reported to the log (once per site/text) and the text is scissor-clipped to
// the bounds instead of spilling. `site` names the call site in diagnostics.
void drawTextFitted(const std::string& text, int x, int y, int maxWidth, int fontSize,
                    Color color, const char* site);

// Word-wrapped block. Draws at most maxLines lines (0 = unlimited) inside
// maxWidth; a truncated block is reported to the log (once per site/text).
// Returns the y just below the last drawn line.
int drawTextWrapped(const std::string& text, int x, int y, int maxWidth, int fontSize,
                    Color color, const char* site, int maxLines = 0);

// The same wrapped block with every line centered on centerX.
int drawTextWrappedCentered(const std::string& text, int centerX, int y, int maxWidth, int fontSize,
                            Color color, const char* site, int maxLines = 0);

// --- M46 procedural UI kit -------------------------------------------------

// Coherent frame variants. One construction language: hard 2px offset shadow
// (elevated variants), ink keyline, middle border, top-left highlight,
// bottom-right depth, 2px stepped ink corners. Accents vary per variant.
enum class FrameStyle {
    Standard,  // plain framed panel
    Raised,    // modal / elevated panel (brighter fill)
    Inset,     // sunken well (list panels, minimap) — inverted bevel
    Danger,    // coral border + top warning notch
    Reward,    // gold border + gold corner pips
    Crystal,   // crystal corner pips (glint on the motion phase)
    Overlay,   // translucent fill, keyline only (over gameplay art)
};

void drawFrame(int x, int y, int w, int h, FrameStyle style);

// Two-frame rhythm (0/1) for the sanctioned micro-motion: chevron nudges,
// crystal glints, danger pulses. Layout and text never move with it; every
// element stays readable captured on either frame.
int motionPhase();

// Pure kit math (unit-tested headlessly in tests/test_ui_kit.cpp).
// Fill width for a meter: 0 at empty, innerWidth at full, never 0 while
// current > 0 (a sliver of life must stay visible).
int meterFillWidth(int current, int maxValue, int innerWidth);
// Picks the gap for a one-row hint layout: gapWide if everything fits,
// else gapNarrow, else -1 (caller falls back to a fitted plain line).
int planHintGap(const std::vector<int>& groupWidths, int gapWide, int gapNarrow, int maxWidth);
// Brightens a color toward white by `amount` per channel (clamped).
Color lighten(Color c, int amount);

// Screen title plaque: a Raised frame with gold end-caps around the measured
// title, centered on centerX. Returns the y just below the plaque.
int drawTitlePlaque(const std::string& title, int centerX, int y, int fontSize);

// Top header band for service screens: panel strip with keyline, centered
// title flanked by two accent pips, and (gold >= 0) a gold badge at the
// right edge. Height is kHeaderBandH.
inline constexpr int kHeaderBandH = 24;
void drawHeaderBand(const std::string& title, int screenW, Color accent, int gold = -1);

// Small section label with a leading accent diamond and a trailing divider
// line to x + w. Returns the y just below.
int drawSectionHeader(const std::string& label, int x, int y, int w);

// Horizontal 1px divider with 2px stepped end blocks.
void drawDivider(int x, int y, int w);

// Selected-row slab: rowFill + rowBorder + 2px gold left end-cap. Menus draw
// this behind the focused row (shape signal #1 of three).
void drawSelectionSlab(int x, int y, int w, int h);

// Stepped right-pointing chevron, 5x7 px (shape signal #2). `nudge` shifts it
// 1px right — pass motionPhase() for the sanctioned idle motion.
void drawChevron(int x, int y, Color color, int nudge = 0);

// Stepped left/right stepper arrows (5x7): dir < 0 left, dir > 0 right.
// Focused arrows draw in the cursor color and alternate 1px on the phase.
void drawStepArrow(int x, int y, int dir, bool focused);

// Value chip: ink outline, chip fill, 2px accent bar, 8px label. Returns the
// chip width. Height is kChipH. drawChipRight aligns the right edge at
// rightX and returns the chip's left x.
inline constexpr int kChipH = 11;
int drawChip(const std::string& label, int x, int y, Color accent);
int drawChipRight(const std::string& label, int rightX, int y, Color accent);

// Compact keycap for control hints: raised fill, top-left 1px highlight,
// gold glyph. Returns the keycap width. Height is kKeycapH.
inline constexpr int kKeycapH = 11;
int drawKeycap(const std::string& key, int x, int y);

// One footer hint group: a keycap ("Enter", "Tab", arrows...) plus a short
// action label. Key strings come from input::primaryLabel — never hard-coded.
struct Hint {
    std::string key;
    std::string label;
};

// Draws the 16px footer strip (fill + keyline) and the hint groups centered
// in it, keycap + label per group. If even the narrow gap cannot fit, falls
// back to one fitted "[key] label" line so nothing silently clips.
void drawFooterHints(const std::vector<Hint>& hints, int screenW, int screenH, const char* site);

// Compact message/notice banner with a shape icon (never color alone):
// Danger = coral triangle-with-! notch, Success = green diamond, Reward =
// gold diamond. Text wraps inside; returns the banner height.
enum class BannerKind { Danger, Success, Reward };
int drawBanner(BannerKind kind, const std::string& text, int x, int y, int w, const char* site);

// Framed meter: ink outline, dark track, capped fill with a 1px top light
// band and segment ticks. HP/MP stay text-labelled by their callers.
void drawMeter(int x, int y, int w, int h, int current, int maxValue, Color fill);

// Focus corner brackets (2px arms) around a rect; expands 1px on the motion
// phase. Visible in grayscale by construction (shape, not hue).
void drawFocusBrackets(int x, int y, int w, int h, Color color);

// Tiny 3x3 crystal pip; the top pixel glints on the motion phase.
void drawCrystalPip(int x, int y);

// Full-screen dim veil under modal panels.
void drawModalDim(int w, int h);

// ---------------------------------------------------------------------------

// Draws menu items top-down from (x, y). The focused row gets the three-part
// selection treatment: slab + chevron + cursor-tinted text.
void drawMenu(const Menu& menu, int x, int y, int itemHeight, int fontSize, Color normal,
              Color disabled, Color cursor);

// Scrolling menu: draws only `visibleRows` items through the window (which
// must have been follow()ed with the menu cursor), with chunky stepped
// more-above/below arrows just outside the list's top/bottom rows. Labels are
// fitted to maxLabelWidth (overflow reported, clipped).
//
// An item's optional `suffix` is drawn right-aligned at x + maxLabelWidth in
// `suffixFontSize` (0 = the row font) and the label is fitted to the room that
// is left, so a trailing cost or count is always fully readable.
void drawMenuScrolled(const Menu& menu, const ScrollWindow& window, int visibleRows, int x, int y,
                      int itemHeight, int fontSize, int maxLabelWidth, Color normal,
                      Color disabled, Color cursor, const char* site, int suffixFontSize = 0,
                      Color suffixColor = Color{150, 175, 235, 255});

}  // namespace cd::ui
