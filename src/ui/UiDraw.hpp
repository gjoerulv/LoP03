#pragma once

#include "raylib.h"
#include "ui/Menu.hpp"

// Small shared raylib drawing helpers for the UI windows. Original 16-bit-style
// framed panels; no copyrighted assets.

namespace cd::ui {

void drawPanel(int x, int y, int w, int h, Color fill, Color border);

void drawTextCentered(const char* text, int centerX, int y, int fontSize, Color color);

// Draws menu items top-down from (x, y). The cursor item is marked and tinted.
void drawMenu(const Menu& menu, int x, int y, int itemHeight, int fontSize, Color normal,
              Color disabled, Color cursor);

}  // namespace cd::ui
