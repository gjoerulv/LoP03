#include "ui/UiDraw.hpp"

namespace cd::ui {

void drawPanel(int x, int y, int w, int h, Color fill, Color border) {
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLines(x, y, w, h, border);
}

void drawTextCentered(const char* text, int centerX, int y, int fontSize, Color color) {
    const int width = MeasureText(text, fontSize);
    DrawText(text, centerX - width / 2, y, fontSize, color);
}

void drawMenu(const Menu& menu, int x, int y, int itemHeight, int fontSize, Color normal,
              Color disabled, Color cursor) {
    const auto& items = menu.items();
    for (std::size_t i = 0; i < items.size(); ++i) {
        const int rowY = y + static_cast<int>(i) * itemHeight;
        const bool isCursor = static_cast<int>(i) == menu.cursor();
        Color color = items[i].enabled ? normal : disabled;
        if (isCursor && items[i].enabled) {
            color = cursor;
            DrawText(">", x - 10, rowY, fontSize, cursor);
        }
        DrawText(items[i].label.c_str(), x, rowY, fontSize, color);
    }
}

}  // namespace cd::ui
