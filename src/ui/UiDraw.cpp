#include "ui/UiDraw.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>

#include "core/Log.hpp"
#include "resource/ResourceManager.hpp"
#include "ui/UiStyle.hpp"

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

// 3x3 accent pip (reads as a tiny diamond at native scale).
void drawPipDiamond(int x, int y, Color c) {
    DrawRectangle(x, y + 1, 3, 1, c);
    DrawRectangle(x + 1, y, 1, 3, c);
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

void drawFramedPanel(ResourceManager& resources, int x, int y, int w, int h, Color fill,
                     Color border) {
    (void)border;
    static const std::string kFrameId = "ui.frame.default";
    if (!resources.hasTexture(kFrameId)) {
        // M46: the procedural fallback IS the house identity now.
        drawFrame(x, y, w, h, FrameStyle::Standard);
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

void drawSceneBackground(ResourceManager& resources, const std::string& id, Color fallback,
                         int w, int h) {
    ClearBackground(fallback);
    if (!resources.hasTexture(id)) {
        return;
    }
    const Texture2D& tex = resources.texture(id);
    DrawTexturePro(tex,
                   Rectangle{0, 0, static_cast<float>(tex.width), static_cast<float>(tex.height)},
                   Rectangle{0, 0, static_cast<float>(w), static_cast<float>(h)}, Vector2{0, 0},
                   0.0f, WHITE);
}

void drawSceneBackground(ResourceManager& resources, const std::string& id, Color fallback,
                         int w, int h, int town) {
    if (town >= 2) {
        const std::string variant = id + "." + std::to_string(town);
        if (resources.hasTexture(variant)) {
            drawSceneBackground(resources, variant, fallback, w, h);
            return;
        }
    }
    drawSceneBackground(resources, id, fallback, w, h);
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

int drawTextWrappedCentered(const std::string& text, int centerX, int y, int maxWidth, int fontSize,
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
        drawTextCentered(line.c_str(), centerX, y + drawn * step, fontSize, color);
        ++drawn;
    }
    return y + drawn * step;
}

// --- M46 procedural UI kit -------------------------------------------------

int motionPhase() { return static_cast<int>(GetTime() * 2.5) & 1; }

int motionPhase3() { return static_cast<int>(GetTime() * 1.5) % 3; }

int meterFillWidth(int current, int maxValue, int innerWidth) {
    if (maxValue <= 0 || current <= 0 || innerWidth <= 0) {
        return 0;
    }
    if (current >= maxValue) {
        return innerWidth;
    }
    const int width = current * innerWidth / maxValue;
    return width < 1 ? 1 : width;
}

int planHintGap(const std::vector<int>& groupWidths, int gapWide, int gapNarrow, int maxWidth) {
    if (groupWidths.empty()) {
        return gapWide;
    }
    int total = 0;
    for (const int w : groupWidths) {
        total += w;
    }
    const int gaps = static_cast<int>(groupWidths.size()) - 1;
    if (total + gaps * gapWide <= maxWidth) {
        return gapWide;
    }
    if (total + gaps * gapNarrow <= maxWidth) {
        return gapNarrow;
    }
    return -1;
}

Color lighten(Color c, int amount) {
    const auto up = [amount](unsigned char v) {
        const int lifted = static_cast<int>(v) + amount;
        return static_cast<unsigned char>(lifted > 255 ? 255 : (lifted < 0 ? 0 : lifted));
    };
    return Color{up(c.r), up(c.g), up(c.b), c.a};
}

void drawFrame(int x, int y, int w, int h, FrameStyle frameStyle) {
    const style::Palette& p = style::palette();
    Color fill = p.panel;
    Color mid = p.borderMid;
    bool shadow = true;
    bool inset = false;
    switch (frameStyle) {
        case FrameStyle::Standard:
            break;
        case FrameStyle::Raised:
            fill = p.panelRaised;
            break;
        case FrameStyle::Inset:
            fill = p.panelInset;
            shadow = false;
            inset = true;
            break;
        case FrameStyle::Danger:
            // The sanctioned pulse: the border alternates two fixed values.
            mid = motionPhase() != 0 ? p.danger : lighten(p.danger, 30);
            break;
        case FrameStyle::Reward:
            mid = p.rowBorder;
            break;
        case FrameStyle::Crystal:
            break;
        case FrameStyle::Overlay:
            shadow = false;
            break;
    }

    if (shadow) {
        // Hard 2px offset shadow, bottom-right — the top-left light rule.
        DrawRectangle(x + 2, y + 2, w, h, p.ink);
    }
    if (frameStyle == FrameStyle::Overlay) {
        DrawRectangle(x, y, w, h, Fade(p.panel, 0.92f));
        DrawRectangleLines(x, y, w, h, p.ink);
    } else {
        DrawRectangle(x, y, w, h, fill);
        DrawRectangleLines(x, y, w, h, p.ink);
        DrawRectangleLines(x + 1, y + 1, w - 2, h - 2, mid);
        const Color topLeft = inset ? p.borderDark : p.borderLight;
        const Color bottomRight = inset ? p.borderLight : p.borderDark;
        DrawRectangle(x + 2, y + 2, w - 4, 1, topLeft);
        DrawRectangle(x + 2, y + 2, 1, h - 4, topLeft);
        DrawRectangle(x + 2, y + h - 3, w - 4, 1, bottomRight);
        DrawRectangle(x + w - 3, y + 2, 1, h - 4, bottomRight);
    }
    // 2px stepped ink corners: the chunky cut-corner signature.
    DrawRectangle(x, y, 2, 2, p.ink);
    DrawRectangle(x + w - 2, y, 2, 2, p.ink);
    DrawRectangle(x, y + h - 2, 2, 2, p.ink);
    DrawRectangle(x + w - 2, y + h - 2, 2, 2, p.ink);

    if (frameStyle == FrameStyle::Danger) {
        // Bold top-center warning tab (shape, not just color).
        const int cx = x + w / 2;
        DrawRectangle(cx - 6, y - 4, 12, 6, p.ink);
        DrawRectangle(cx - 5, y - 3, 10, 4, p.danger);
        DrawRectangle(cx - 1, y - 2, 2, 2, p.ink);
    } else if (frameStyle == FrameStyle::Reward) {
        drawPipDiamond(x + 4, y + 4, p.gold);
        drawPipDiamond(x + w - 7, y + 4, p.gold);
        drawPipDiamond(x + 4, y + h - 7, p.gold);
        drawPipDiamond(x + w - 7, y + h - 7, p.gold);
    } else if (frameStyle == FrameStyle::Crystal) {
        drawCrystalPip(x + 4, y + 4);
        drawCrystalPip(x + w - 7, y + 4);
        drawCrystalPip(x + 4, y + h - 7);
        drawCrystalPip(x + w - 7, y + h - 7);
    }
}

int drawTitlePlaque(const std::string& title, int centerX, int y, int fontSize) {
    const style::Palette& p = style::palette();
    const int tw = measureText(title, fontSize);
    const int pw = tw + 34;
    const int ph = fontSize + 12;
    const int px = centerX - pw / 2;
    drawFrame(px, y, pw, ph, FrameStyle::Raised);
    DrawRectangle(px + 4, y + 4, 2, ph - 8, p.rowBorder);
    DrawRectangle(px + pw - 6, y + 4, 2, ph - 8, p.rowBorder);
    drawText(title, centerX - tw / 2, y + 6, fontSize, p.text);
    return y + ph;
}

void drawHeaderBand(const std::string& title, int screenW, Color accent, int gold) {
    const style::Palette& p = style::palette();
    DrawRectangle(0, 0, screenW, kHeaderBandH, p.panel);
    DrawRectangle(0, 0, screenW, 1, p.borderLight);
    DrawRectangle(0, kHeaderBandH - 2, screenW, 1, p.borderDark);
    DrawRectangle(0, kHeaderBandH - 1, screenW, 1, p.ink);
    const int tw = measureText(title, style::kFontScreenTitle);
    drawText(title, screenW / 2 - tw / 2, 4, style::kFontScreenTitle, p.text);
    drawPipDiamond(screenW / 2 - tw / 2 - 10, 9, accent);
    drawPipDiamond(screenW / 2 + tw / 2 + 7, 9, accent);
    if (gold >= 0) {
        drawChipRight(std::to_string(gold) + "g", screenW - 4, 6, p.gold);
    }
}

int drawSectionHeader(const std::string& label, int x, int y, int w) {
    const style::Palette& p = style::palette();
    drawPipDiamond(x, y + 2, p.gold);
    drawText(label, x + 6, y, style::kFontSmall, p.textDim);
    const int tw = measureText(label, style::kFontSmall);
    const int lineX = x + 6 + tw + 5;
    if (lineX < x + w - 2) {
        DrawRectangle(lineX, y + 3, x + w - lineX, 1, p.borderDark);
    }
    return y + 10;
}

void drawDivider(int x, int y, int w) {
    const style::Palette& p = style::palette();
    DrawRectangle(x + 2, y, w - 4, 1, p.borderMid);
    DrawRectangle(x, y - 1, 2, 3, p.borderDark);
    DrawRectangle(x + w - 2, y - 1, 2, 3, p.borderDark);
}

void drawSelectionSlab(int x, int y, int w, int h) {
    const style::Palette& p = style::palette();
    DrawRectangle(x, y, w, h, p.rowFill);
    DrawRectangleLines(x, y, w, h, p.rowBorder);
    DrawRectangle(x, y, 2, h, p.rowBorder);
}

void drawChevron(int x, int y, Color color, int nudge) {
    x += nudge;
    DrawRectangle(x, y, 2, 7, color);
    DrawRectangle(x + 2, y + 2, 2, 3, color);
    DrawRectangle(x + 4, y + 3, 1, 1, color);
}

void drawStepArrow(int x, int y, int dir, bool focused) {
    const style::Palette& p = style::palette();
    const Color c = focused ? p.cursor : p.textDim;
    if (focused) {
        x += motionPhase() * (dir > 0 ? 1 : -1);
    }
    if (dir > 0) {
        DrawRectangle(x, y, 2, 7, c);
        DrawRectangle(x + 2, y + 2, 2, 3, c);
        DrawRectangle(x + 4, y + 3, 1, 1, c);
    } else {
        DrawRectangle(x + 3, y, 2, 7, c);
        DrawRectangle(x + 1, y + 2, 2, 3, c);
        DrawRectangle(x, y + 3, 1, 1, c);
    }
}

int drawChip(const std::string& label, int x, int y, Color accent) {
    const style::Palette& p = style::palette();
    const int tw = measureText(label, style::kFontSmall);
    const int cw = tw + 10;
    DrawRectangle(x, y, cw, kChipH, p.ink);
    DrawRectangle(x + 1, y + 1, cw - 2, kChipH - 2, p.chipFill);
    DrawRectangle(x + 1, y + 1, 2, kChipH - 2, accent);
    drawText(label, x + 6, y + 2, style::kFontSmall, p.text);
    return cw;
}

int drawChipRight(const std::string& label, int rightX, int y, Color accent) {
    const int cw = measureText(label, style::kFontSmall) + 10;
    const int x = rightX - cw;
    drawChip(label, x, y, accent);
    return x;
}

int drawKeycap(const std::string& key, int x, int y) {
    const style::Palette& p = style::palette();
    const int tw = measureText(key, style::kFontSmall);
    const int kw = tw + 8;
    DrawRectangle(x, y, kw, kKeycapH, p.ink);
    DrawRectangle(x + 1, y + 1, kw - 2, kKeycapH - 2, p.keycapFill);
    DrawRectangle(x + 1, y + 1, kw - 2, 1, p.borderLight);
    DrawRectangle(x + 1, y + 1, 1, kKeycapH - 2, p.borderLight);
    DrawRectangle(x + 2, y + kKeycapH - 2, kw - 3, 1, p.borderDark);
    drawText(key, x + 4, y + 2, style::kFontSmall, p.gold);
    return kw;
}

void drawFooterHints(const std::vector<Hint>& hints, int screenW, int screenH, const char* site) {
    const style::Palette& p = style::palette();
    const int stripY = screenH - style::kFooterHeight;
    DrawRectangle(0, stripY, screenW, style::kFooterHeight, p.footerFill);
    DrawRectangle(0, stripY, screenW, 1, p.ink);
    if (hints.empty()) {
        return;
    }
    std::vector<int> widths;
    widths.reserve(hints.size());
    for (const Hint& hint : hints) {
        widths.push_back(measureText(hint.key, style::kFontSmall) + 8 + 3 +
                         measureText(hint.label, style::kFontSmall));
    }
    const int gap = planHintGap(widths, 12, 6, screenW - 8);
    if (gap < 0) {
        // Prioritized compact fallback: one fitted plain line, never a silent clip.
        std::string line;
        for (const Hint& hint : hints) {
            if (!line.empty()) {
                line += "  ";
            }
            line += "[" + hint.key + "] " + hint.label;
        }
        drawTextFitted(line, 4, stripY + 4, screenW - 8, style::kFontSmall, p.textHint, site);
        return;
    }
    int total = gap * (static_cast<int>(hints.size()) - 1);
    for (const int w : widths) {
        total += w;
    }
    int x = (screenW - total) / 2;
    const int keyY = stripY + 2;
    for (std::size_t i = 0; i < hints.size(); ++i) {
        const int kw = drawKeycap(hints[i].key, x, keyY);
        drawText(hints[i].label, x + kw + 3, keyY + 2, style::kFontSmall, p.textHint);
        x += widths[static_cast<std::size_t>(i)] + gap;
    }
}

int drawBanner(BannerKind kind, const std::string& text, int x, int y, int w, const char* site) {
    const style::Palette& p = style::palette();
    Color accent = p.danger;
    if (kind == BannerKind::Success) {
        accent = p.success;
    } else if (kind == BannerKind::Reward) {
        accent = p.gold;
    }
    const int textX = x + 20;
    const int textW = w - 26;
    const std::vector<std::string> lines =
        wrapText(text, textW, style::kFontBody, raylibMeasure());
    const int bh = std::max(17, static_cast<int>(lines.size()) * lineHeight(style::kFontBody) + 6);
    DrawRectangle(x, y, w, bh, p.panel);
    DrawRectangleLines(x, y, w, bh, p.ink);
    DrawRectangleLines(x + 1, y + 1, w - 2, bh - 2, accent);
    DrawRectangle(x, y, 2, 2, p.ink);
    DrawRectangle(x + w - 2, y, 2, 2, p.ink);
    DrawRectangle(x, y + bh - 2, 2, 2, p.ink);
    DrawRectangle(x + w - 2, y + bh - 2, 2, 2, p.ink);

    const int icx = x + 10;
    const int icy = y + bh / 2;
    if (kind == BannerKind::Danger) {
        // Stepped warning triangle with an ink '!' — readable in grayscale.
        DrawRectangle(icx - 1, icy - 3, 2, 2, accent);
        DrawRectangle(icx - 3, icy - 1, 6, 2, accent);
        DrawRectangle(icx - 5, icy + 1, 10, 2, accent);
        DrawRectangle(icx - 1, icy, 2, 2, p.ink);
    } else {
        // Stepped diamond for good news.
        DrawRectangle(icx - 1, icy - 3, 2, 2, accent);
        DrawRectangle(icx - 3, icy - 1, 6, 2, accent);
        DrawRectangle(icx - 1, icy + 1, 2, 2, accent);
    }
    drawTextWrapped(text, textX, y + 4, textW, style::kFontBody, p.text, site);
    return bh;
}

void drawMeter(int x, int y, int w, int h, int current, int maxValue, Color fill) {
    const style::Palette& p = style::palette();
    DrawRectangle(x, y, w, h, p.ink);
    DrawRectangle(x + 1, y + 1, w - 2, h - 2, p.meterBack);
    const int fw = meterFillWidth(current, maxValue, w - 2);
    if (fw > 0) {
        DrawRectangle(x + 1, y + 1, fw, h - 2, fill);
        DrawRectangle(x + 1, y + 1, fw, 1, lighten(fill, 48));
        DrawRectangle(x + fw, y + 1, 1, h - 2, lighten(fill, 72));
    }
    // Segment ticks keep long bars readable at a glance.
    for (int tx = x + 8; tx < x + w - 1; tx += 8) {
        DrawRectangle(tx, y + h - 2, 1, 1, p.meterBack);
    }
}

void drawFocusBrackets(int x, int y, int w, int h, Color color) {
    const int e = motionPhase();
    x -= 2 + e;
    y -= 2 + e;
    w += 4 + 2 * e;
    h += 4 + 2 * e;
    constexpr int kArm = 5;
    DrawRectangle(x, y, kArm, 2, color);
    DrawRectangle(x, y, 2, kArm, color);
    DrawRectangle(x + w - kArm, y, kArm, 2, color);
    DrawRectangle(x + w - 2, y, 2, kArm, color);
    DrawRectangle(x, y + h - 2, kArm, 2, color);
    DrawRectangle(x, y + h - kArm, 2, kArm, color);
    DrawRectangle(x + w - kArm, y + h - 2, kArm, 2, color);
    DrawRectangle(x + w - 2, y + h - kArm, 2, kArm, color);
}

void drawCrystalPip(int x, int y) {
    const style::Palette& p = style::palette();
    DrawRectangle(x, y + 1, 3, 1, p.crystal);
    DrawRectangle(x + 1, y, 1, 3, p.crystal);
    if (motionPhase() != 0) {
        DrawRectangle(x + 1, y, 1, 1, p.borderLight);
    }
}

void drawModalDim(int w, int h) { DrawRectangle(0, 0, w, h, style::palette().modalDim); }

// ---------------------------------------------------------------------------

void drawMenu(const Menu& menu, int x, int y, int itemHeight, int fontSize, Color normal,
              Color disabled, Color cursor) {
    const auto& items = menu.items();
    int labelW = 0;
    for (const MenuItem& item : items) {
        labelW = std::max(labelW, measureWidth(item.label.c_str(), fontSize));
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        const int rowY = y + static_cast<int>(i) * itemHeight;
        const bool isCursor = static_cast<int>(i) == menu.cursor();
        Color color = items[i].enabled ? normal : disabled;
        if (isCursor && items[i].enabled) {
            color = cursor;
            const int slabH = std::min(itemHeight + 1, fontSize + 6);
            drawSelectionSlab(x - 12, rowY - 2, labelW + 18, slabH);
            drawChevron(x - 9, rowY + (fontSize - 8) / 2, cursor, motionPhase());
        }
        drawTextRaw(items[i].label.c_str(), x, rowY, fontSize, color);
    }
}

void drawMenuScrolled(const Menu& menu, const ScrollWindow& window, int visibleRows, int x, int y,
                      int itemHeight, int fontSize, int maxLabelWidth, Color normal,
                      Color disabled, Color cursor, const char* site, int suffixFontSize,
                      Color suffixColor) {
    const auto& items = menu.items();
    const int total = static_cast<int>(items.size());
    const int first = window.top();
    const int count = window.visibleCount(total, visibleRows);
    const int suffixFont = suffixFontSize > 0 ? suffixFontSize : fontSize;

    for (int row = 0; row < count; ++row) {
        const MenuItem& item = items[static_cast<std::size_t>(first + row)];
        const int rowY = y + row * itemHeight;
        const bool isCursor = first + row == menu.cursor();
        Color color = item.enabled ? normal : disabled;
        if (isCursor && item.enabled) {
            color = cursor;
            const int slabH = std::min(itemHeight + 1, fontSize + 6);
            drawSelectionSlab(x - 12, rowY - 2, maxLabelWidth + 18, slabH);
            drawChevron(x - 9, rowY + (fontSize - 8) / 2, cursor, motionPhase());
        }
        // The suffix column is reserved first; the label takes what is left.
        int labelWidth = maxLabelWidth;
        if (!item.suffix.empty()) {
            const int suffixWidth = measureWidth(item.suffix.c_str(), suffixFont);
            labelWidth = maxLabelWidth - suffixWidth - 6;
            if (labelWidth < 8) {
                labelWidth = 8;
            }
            drawTextRaw(item.suffix.c_str(), x + maxLabelWidth - suffixWidth,
                        rowY + (fontSize - suffixFont), suffixFont,
                        item.enabled ? suffixColor : disabled);
        }
        drawTextFitted(item.label, x, rowY, labelWidth, fontSize, color, site);
    }

    // Chunky stepped more-above/below arrows right of the list.
    const int ax = x + maxLabelWidth + 6;
    if (window.moreAbove()) {
        DrawRectangle(ax + 2, y - 5, 4, 2, normal);
        DrawRectangle(ax, y - 3, 8, 2, normal);
    }
    if (window.moreBelow(total, visibleRows)) {
        const int by = y + count * itemHeight - itemHeight + fontSize;
        DrawRectangle(ax, by - 2, 8, 2, normal);
        DrawRectangle(ax + 2, by, 4, 2, normal);
    }
}

}  // namespace cd::ui
