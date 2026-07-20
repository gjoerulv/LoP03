#include "ui/UiDraw.hpp"

#include <string>
#include <unordered_set>

#include "core/Log.hpp"
#include "resource/ResourceManager.hpp"

namespace cd::ui {

namespace {

// Total overflow/truncation occurrences (every event, not deduplicated);
// the M23 capture tool asserts a zero delta per rendered scene.
long gOverflowEvents = 0;

// Active bitmap fonts (M25). Non-owning; installed by setFonts after each
// manifest load. Null until then -> the default font is used, so headless and
// pre-load paths never crash.
const Font* gFontSmall = nullptr;
const Font* gFontMain = nullptr;
const Font* gFontTitle = nullptr;

// Spacing is baked into each glyph's advance, so the DrawTextEx/MeasureTextEx
// inter-glyph spacing is zero — one convention, in one place.
constexpr float kSpacing = 0.0f;

// Picks the base font whose native size is nearest the requested size so pixel
// glyphs render close to 1:1 (small=8, main=10, title=20). Falls back to the
// default font whenever the chosen slot is not installed.
const Font& activeFont(int fontSize) {
    const Font* chosen = fontSize <= 9 ? gFontSmall : (fontSize <= 15 ? gFontMain : gFontTitle);
    if (chosen == nullptr) {
        chosen = gFontMain != nullptr ? gFontMain : (gFontSmall != nullptr ? gFontSmall : gFontTitle);
    }
    static const Font fallback = GetFontDefault();
    return chosen != nullptr ? *chosen : fallback;
}

void drawTextRaw(const char* text, int x, int y, int fontSize, Color color) {
    DrawTextEx(activeFont(fontSize), text, Vector2{static_cast<float>(x), static_cast<float>(y)},
               static_cast<float>(fontSize), kSpacing, color);
}

int measureWidth(const char* text, int fontSize) {
    return static_cast<int>(
        MeasureTextEx(activeFont(fontSize), text, static_cast<float>(fontSize), kSpacing).x);
}

// Overflow diagnostics: report each unique (site, text) once so a per-frame
// render loop cannot spam the log. Small and bounded by the game's own text.
void reportOverflowOnce(const char* site, const std::string& text, int width, int maxWidth) {
    ++gOverflowEvents;
    static std::unordered_set<std::string> reported;
    std::string key = std::string(site) + "|" + text;
    if (reported.insert(std::move(key)).second) {
        log::warn("[ui-overflow] " + std::string(site) + ": '" + text + "' " +
                  std::to_string(width) + "px > " + std::to_string(maxWidth) + "px");
    }
}

}  // namespace

long overflowEvents() { return gOverflowEvents; }

void setFonts(const Font* small, const Font* main, const Font* title) {
    gFontSmall = small;
    gFontMain = main;
    gFontTitle = title;
}

void drawText(const std::string& text, int x, int y, int fontSize, Color color) {
    drawTextRaw(text.c_str(), x, y, fontSize, color);
}

void drawText(const char* text, int x, int y, int fontSize, Color color) {
    drawTextRaw(text, x, y, fontSize, color);
}

void drawPanel(int x, int y, int w, int h, Color fill, Color border) {
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLines(x, y, w, h, border);
    // A darker inner frame gives a layered 16-bit window look.
    const Color inner{static_cast<unsigned char>(border.r * 3 / 5),
                      static_cast<unsigned char>(border.g * 3 / 5),
                      static_cast<unsigned char>(border.b * 3 / 5), border.a};
    DrawRectangleLines(x + 2, y + 2, w - 4, h - 4, inner);
}

void drawFramedPanel(ResourceManager& resources, int x, int y, int w, int h, Color fill,
                     Color border) {
    static const std::string kFrameId = "ui.frame.default";
    if (!resources.hasTexture(kFrameId)) {
        drawPanel(x, y, w, h, fill, border);
        return;
    }
    DrawRectangle(x + 3, y + 3, w - 6, h - 6, fill);
    const Texture2D& frame = resources.texture(kFrameId);
    NPatchInfo patch{};
    patch.source = Rectangle{0, 0, static_cast<float>(frame.width),
                             static_cast<float>(frame.height)};
    patch.left = 8;
    patch.top = 8;
    patch.right = 8;
    patch.bottom = 8;
    patch.layout = NPATCH_NINE_PATCH;
    DrawTextureNPatch(frame, patch,
                      Rectangle{static_cast<float>(x), static_cast<float>(y),
                                static_cast<float>(w), static_cast<float>(h)},
                      Vector2{0, 0}, 0.0f, WHITE);
}

int measureText(const std::string& text, int fontSize) {
    return measureWidth(text.c_str(), fontSize);
}

const TextMeasure& raylibMeasure() {
    static const TextMeasure measure = [](const std::string& text, int fontSize) {
        return measureWidth(text.c_str(), fontSize);
    };
    return measure;
}

void drawTextCentered(const char* text, int centerX, int y, int fontSize, Color color) {
    const int width = measureWidth(text, fontSize);
    drawTextRaw(text, centerX - width / 2, y, fontSize, color);
}

void drawTextRight(const std::string& text, int rightX, int y, int fontSize, Color color) {
    const int width = measureWidth(text.c_str(), fontSize);
    drawTextRaw(text.c_str(), rightX - width, y, fontSize, color);
}

void drawTextFitted(const std::string& text, int x, int y, int maxWidth, int fontSize,
                    Color color, const char* site) {
    const int width = measureWidth(text.c_str(), fontSize);
    if (width <= maxWidth) {
        drawTextRaw(text.c_str(), x, y, fontSize, color);
        return;
    }
    reportOverflowOnce(site, text, width, maxWidth);
    BeginScissorMode(x, y, maxWidth, lineHeight(fontSize));
    drawTextRaw(text.c_str(), x, y, fontSize, color);
    EndScissorMode();
}

int drawTextWrapped(const std::string& text, int x, int y, int maxWidth, int fontSize,
                    Color color, const char* site, int maxLines) {
    const std::vector<std::string> lines = wrapText(text, maxWidth, fontSize, raylibMeasure());
    const int step = lineHeight(fontSize);
    int drawn = 0;
    for (const std::string& line : lines) {
        if (maxLines > 0 && drawn >= maxLines) {
            reportOverflowOnce(site, text, static_cast<int>(lines.size()) * step,
                               maxLines * step);
            break;
        }
        drawTextRaw(line.c_str(), x, y + drawn * step, fontSize, color);
        ++drawn;
    }
    return y + drawn * step;
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
            drawTextRaw(">", x - 10, rowY, fontSize, cursor);
        }
        drawTextRaw(items[i].label.c_str(), x, rowY, fontSize, color);
    }
}

void drawMenuScrolled(const Menu& menu, const ScrollWindow& window, int visibleRows, int x, int y,
                      int itemHeight, int fontSize, int maxLabelWidth, Color normal,
                      Color disabled, Color cursor, const char* site) {
    const auto& items = menu.items();
    const int total = static_cast<int>(items.size());
    const int first = window.top();
    const int count = window.visibleCount(total, visibleRows);

    for (int row = 0; row < count; ++row) {
        const int i = first + row;
        const int rowY = y + row * itemHeight;
        const bool isCursor = i == menu.cursor();
        Color color = items[static_cast<std::size_t>(i)].enabled ? normal : disabled;
        if (isCursor && items[static_cast<std::size_t>(i)].enabled) {
            color = cursor;
            drawTextRaw(">", x - 10, rowY, fontSize, cursor);
        }
        drawTextFitted(items[static_cast<std::size_t>(i)].label, x, rowY, maxLabelWidth, fontSize,
                       color, site);
    }

    // More-above/below arrows, drawn as small triangles right of the list.
    const int ax = x + maxLabelWidth + 6;
    if (window.moreAbove()) {
        DrawTriangle(Vector2{static_cast<float>(ax + 3), static_cast<float>(y - 2)},
                     Vector2{static_cast<float>(ax), static_cast<float>(y + 3)},
                     Vector2{static_cast<float>(ax + 6), static_cast<float>(y + 3)}, normal);
    }
    if (window.moreBelow(total, visibleRows)) {
        const int by = y + count * itemHeight - itemHeight + fontSize;
        DrawTriangle(Vector2{static_cast<float>(ax), static_cast<float>(by - 3)},
                     Vector2{static_cast<float>(ax + 3), static_cast<float>(by + 2)},
                     Vector2{static_cast<float>(ax + 6), static_cast<float>(by - 3)}, normal);
    }
}

}  // namespace cd::ui
