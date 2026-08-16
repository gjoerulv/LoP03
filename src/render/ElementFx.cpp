#include "render/ElementFx.hpp"

#include "raylib.h"
#include "ui/UiStyle.hpp"

namespace cd::render {

namespace {

// Fixed element hues, readable on the dark canvas (fire/holy warm, ice/
// lightning cold, earth dun, dark violet). High contrast collapses to the
// palette text color so shape alone carries the read.
Color elementHue(content::Element e, bool highContrast) {
    if (highContrast) {
        return ui::style::palette().text;
    }
    switch (e) {
        case content::Element::Fire: return Color{232, 120, 60, 255};
        case content::Element::Ice: return Color{140, 200, 236, 255};
        case content::Element::Lightning: return Color{240, 224, 96, 255};
        case content::Element::Earth: return Color{176, 132, 84, 255};
        case content::Element::Holy: return Color{248, 240, 180, 255};
        case content::Element::Dark: return Color{164, 108, 216, 255};
        case content::Element::None: break;
    }
    return ui::style::palette().text;
}

void px(int x, int y, int w, int h, Color c) { DrawRectangle(x, y, w, h, c); }

}  // namespace

void drawElementImpact(content::Element element, int cx, int cy, float strength,
                       bool highContrast) {
    if (element == content::Element::None || strength <= 0.01f) {
        return;  // no element, or the Battle Flash gate is closed
    }
    const Color c = Fade(elementHue(element, highContrast), strength);
    const Color dim = Fade(elementHue(element, highContrast), strength * 0.5f);
    // The motif "grows" one step as the pulse peaks — derived from strength
    // alone, so the two-frame capture clock and replays are stable.
    const int g = strength > 0.6f ? 1 : 0;

    switch (element) {
        case content::Element::Fire:
            // Three stepped flame wedges licking upward.
            px(cx - 7, cy + 2, 3, 4, dim);
            px(cx - 6, cy - 2 - g, 2, 5, c);
            px(cx - 1, cy - 6 - g, 3, 9, c);
            px(cx, cy - 9 - g * 2, 2, 4, dim);
            px(cx + 5, cy - 3 - g, 2, 6, c);
            px(cx + 6, cy + 2, 3, 3, dim);
            break;
        case content::Element::Ice:
            // Four diagonal shards in an X, tips brightest.
            for (int i = 0; i < 4; ++i) {
                const int sx = i % 2 == 0 ? -1 : 1;
                const int sy = i < 2 ? -1 : 1;
                px(cx + sx * (4 + g), cy + sy * (4 + g), 2, 2, c);
                px(cx + sx * (7 + g), cy + sy * (7 + g), 3, 3, dim);
            }
            px(cx - 1, cy - 1, 2, 2, c);
            break;
        case content::Element::Lightning:
            // One stepped bolt, upper-right to lower-left.
            px(cx + 4, cy - 9 - g, 3, 4, dim);
            px(cx + 1, cy - 6, 4, 4, c);
            px(cx - 2, cy - 2, 5, 3, c);
            px(cx - 5, cy + 1, 4, 4, c);
            px(cx - 8, cy + 5 + g, 3, 4, dim);
            break;
        case content::Element::Earth:
            // Low rubble chips bouncing off the ground line.
            px(cx - 8, cy + 4, 4, 3, c);
            px(cx - 2, cy + 2 - g, 5, 4, c);
            px(cx + 5, cy + 4, 3, 3, dim);
            px(cx - 5, cy - 1 - g, 3, 2, dim);
            px(cx + 2, cy - 2 - g, 2, 2, dim);
            break;
        case content::Element::Holy:
            // A radiant plus with faint diagonal seconds.
            px(cx - 1, cy - 9 - g, 2, 6, c);
            px(cx - 1, cy + 3, 2, 6 + g, c);
            px(cx - 9 - g, cy - 1, 6, 2, c);
            px(cx + 3, cy - 1, 6 + g, 2, c);
            px(cx - 5, cy - 5, 2, 2, dim);
            px(cx + 3, cy - 5, 2, 2, dim);
            px(cx - 5, cy + 3, 2, 2, dim);
            px(cx + 3, cy + 3, 2, 2, dim);
            break;
        case content::Element::Dark:
            // A broken wisp ring — presence by absence.
            px(cx - 7 - g, cy - 3, 3, 2, c);
            px(cx - 4, cy - 7 - g, 3, 2, dim);
            px(cx + 2, cy - 6, 3, 2, c);
            px(cx + 6 + g, cy - 1, 2, 3, dim);
            px(cx + 3, cy + 5 + g, 3, 2, c);
            px(cx - 5, cy + 4, 3, 2, dim);
            break;
        case content::Element::None:
            break;
    }
}

}  // namespace cd::render
