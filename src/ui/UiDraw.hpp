#pragma once

#include <string>

#include "raylib.h"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"
#include "ui/TextLayout.hpp"

// Small shared raylib drawing helpers for the UI windows. Original 16-bit-style
// framed panels; no copyrighted assets. Text placement is measured (M12):
// bounded helpers report overflow to the log once per site instead of
// silently clipping or spilling.

namespace cd {
class ResourceManager;
}

namespace cd::ui {

void drawPanel(int x, int y, int w, int h, Color fill, Color border);

// Running count of text overflow/truncation events (every occurrence). The
// M23 capture tool asserts a zero delta per rendered scene; tests may too.
long overflowEvents();

// Panel that uses the nine-patch "ui.frame.default" texture when the catalog
// provides one (M15), and the plain double-border drawPanel otherwise.
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

// Draws menu items top-down from (x, y). The cursor item is marked and tinted.
void drawMenu(const Menu& menu, int x, int y, int itemHeight, int fontSize, Color normal,
              Color disabled, Color cursor);

// Scrolling menu: draws only `visibleRows` items through the window (which
// must have been follow()ed with the menu cursor), with more-above/below
// arrows just outside the list's top/bottom rows. Labels are fitted to
// maxLabelWidth (overflow reported, clipped).
void drawMenuScrolled(const Menu& menu, const ScrollWindow& window, int visibleRows, int x, int y,
                      int itemHeight, int fontSize, int maxLabelWidth, Color normal,
                      Color disabled, Color cursor, const char* site);

}  // namespace cd::ui
