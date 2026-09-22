#include "render/ActionFx.hpp"

#include <algorithm>

#include "ui/UiStyle.hpp"

namespace cd::render {

namespace {

void px(int x, int y, int w, int h, Color c) { DrawRectangle(x, y, w, h, c); }

// A 2x2 pixel block — the unit of every motif here, so nothing is finer
// than the sprites it lands on.
void blk(int x, int y, Color c) { DrawRectangle(x, y, 2, 2, c); }

int spread(ActionTier tier) {
    switch (tier) {
        case ActionTier::Minor: return 0;
        case ActionTier::Major: return 1;
        case ActionTier::Grand: return 2;
    }
    return 0;
}

// A five-pixel chevron (an upward or downward arrowhead), the buff idiom.
void chevron(int cx, int y, bool up, Color c) {
    const int d = up ? 1 : -1;
    px(cx - 3, y + d, 2, 1, c);
    px(cx - 1, y, 2, 1, c);
    px(cx + 1, y + d, 2, 1, c);
}

}  // namespace

Color actionFamilyColor(content::SkillKind kind, bool highContrast) {
    const ui::style::Palette& pal = ui::style::palette();
    if (highContrast) {
        return pal.text;
    }
    switch (kind) {
        case content::SkillKind::Fire: return Color{232, 120, 60, 255};
        case content::SkillKind::Ice: return Color{140, 200, 236, 255};
        case content::SkillKind::Lightning: return Color{240, 224, 96, 255};
        case content::SkillKind::Earth: return Color{176, 132, 84, 255};
        case content::SkillKind::Holy: return Color{248, 240, 180, 255};
        case content::SkillKind::Dark: return Color{164, 108, 216, 255};
        case content::SkillKind::NonElemental: return pal.text;
        case content::SkillKind::Heal: return pal.success;
        case content::SkillKind::Buff: return pal.gold;
        case content::SkillKind::Debuff: return pal.magic;
        case content::SkillKind::Summon: break;
    }
    return pal.text;
}

void drawActionCast(content::SkillKind kind, ActionTier tier, int cx, int topY, float t,
                    bool highContrast) {
    if (t < 0.0f || kind == content::SkillKind::NonElemental || kind == content::SkillKind::Summon) {
        return;
    }
    const Color c = actionFamilyColor(kind, highContrast);
    const Color dim = Fade(c, 0.5f);
    // Three motes rising in a stepped arc over the caster, one more per tier
    // step; the pixel steps come from the clock, never from smoothing.
    const int f = frameIndex(t, 4, kActionFrameTime);
    const int count = 3 + spread(tier);
    for (int i = 0; i < count; ++i) {
        const int x = cx - 6 + i * 3 + ((i + f) % 2);
        const int y = topY - 2 - f * 2 - (i % 2) * 2;
        blk(x, y, (i + f) % 2 == 0 ? c : dim);
    }
    if (kind == content::SkillKind::Heal) {
        px(cx - 1, topY - 8 - f, 2, 4, c);  // a small cross for the mender
        px(cx - 2, topY - 7 - f, 4, 2, c);
    }
}

void drawActionBurst(content::SkillKind kind, ActionTier tier, int cx, int cy, float t,
                     bool highContrast) {
    if (t < 0.0f || kind == content::SkillKind::Summon) {
        return;
    }
    const int frames = actionFrames(tier);
    const float framesLen = static_cast<float>(frames) * kActionFrameTime;
    const float tail = actionTailSeconds(tier);
    if (t >= framesLen + tail) {
        return;
    }
    // Alpha: full through the frames, then one linear fade through the tail
    // (a decay, never a strobe).
    const float alpha = t <= framesLen ? 1.0f : std::max(0.0f, 1.0f - (t - framesLen) / tail);
    const int f = frameIndex(t, frames, kActionFrameTime);
    const int last = frames - 1;
    const int s = spread(tier);
    const Color c = Fade(actionFamilyColor(kind, highContrast), alpha);
    const Color dim = Fade(actionFamilyColor(kind, highContrast), alpha * 0.5f);

    switch (kind) {
        case content::SkillKind::NonElemental: {
            // A stepped slash band, lower-left to upper-right, that breaks into
            // chips: frame 0 the first half, frame 1 the whole band, then chips.
            const int len = 6 + s * 2;
            const int shown = f == 0 ? len / 2 : len;
            if (f <= 1 + s) {
                for (int i = 0; i < shown; ++i) {
                    blk(cx - len + i * 2, cy + len / 2 - i * 2 + 2, i % 3 == 0 ? dim : c);
                }
            } else {
                const int k = f - 1 - s;  // chips flying out
                blk(cx + 4 + k * 3, cy - 6 - k * 2, c);
                blk(cx - 6 - k * 2, cy + 4 + k, dim);
                blk(cx + 1, cy - 9 - k * 3, dim);
            }
            break;
        }
        case content::SkillKind::Fire: {
            // Stepped flame columns rising from the unit's feet to its belly —
            // the head stays readable; more columns per tier. The core is
            // bright, the flanks dim.
            const int cols = 3 + s * 2;
            const int base = cy + 12;
            for (int i = 0; i < cols; ++i) {
                const int x = cx - cols + i * 2 - s * 2 + (i % 2 == 0 ? 0 : 1);
                const int h = std::min(12, 3 + f * 2 + (i % 2) * 2 - (f == last ? 2 : 0));
                px(x, base - h, 2, h, i % 2 == 0 ? c : dim);
                blk(x, base - h - 2 - (f % 2), dim);  // a lick above each column
            }
            break;
        }
        case content::SkillKind::Ice: {
            // Shards falling in from above, then a splash of chips at the feet.
            const int n = 4 + s * 2;
            for (int i = 0; i < n; ++i) {
                const int x = cx - n * 2 + i * 4 + (i % 2);
                if (f < last) {
                    const int y = cy - 16 - (i % 3) * 3 + f * 5;
                    px(x, y, 2, 4, i % 2 == 0 ? c : dim);
                } else {
                    blk(x, cy + 6 - (i % 2) * 3, dim);
                }
            }
            break;
        }
        case content::SkillKind::Lightning: {
            // A jagged bolt from the top edge to the unit: thin, then thick with
            // a branch, then thin again as it fades.
            const int w = f == 1 || f == 2 ? 2 : 1;
            int x = cx + 4;
            for (int y = cy - 24; y < cy - 2; y += 3) {
                px(x, y, w, 3, c);
                x += ((y / 3) % 2 == 0) ? -3 : 2;
            }
            if (f >= 1 && f <= 2 + s) {
                px(cx - 6, cy - 14, 1, 6, dim);  // the branch
                px(cx - 8, cy - 9, 3, 1, dim);
            }
            for (int i = 0; i < 3 + s; ++i) {
                blk(cx - 6 + i * 4, cy + 4 - (f % 2) * 2, i % 2 == 0 ? dim : c);  // ground sparks
            }
            break;
        }
        case content::SkillKind::Earth: {
            // Rubble chips thrown up, then falling back — a stepped parabola.
            const int n = 5 + s * 2;
            for (int i = 0; i < n; ++i) {
                const int x = cx - n * 2 + i * 4 + ((i * 7) % 3);
                const int peak = 4 + (i % 3) * 3;
                const int rise = f <= last / 2 ? f * 3 : (last - f) * 3;
                const int y = cy + 8 - std::min(rise, peak);
                blk(x, y, i % 2 == 0 ? c : dim);
            }
            break;
        }
        case content::SkillKind::Holy: {
            // A plus expanding with a ring of diagonal pips.
            const int r = 3 + f * 3 + s * 2;
            px(cx - 1, cy - r, 2, r * 2, f == last ? dim : c);
            px(cx - r, cy - 1, r * 2, 2, f == last ? dim : c);
            const int d = (r * 7) / 10;
            blk(cx - d, cy - d, dim);
            blk(cx + d - 2, cy - d, dim);
            blk(cx - d, cy + d - 2, dim);
            blk(cx + d - 2, cy + d - 2, dim);
            break;
        }
        case content::SkillKind::Dark: {
            // A ring of wisps collapsing onto the unit.
            const int r = std::max(2, 12 + s * 3 - f * 3);
            const int pts[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
            for (int i = 0; i < 8; ++i) {
                const int rr = (i % 2 == 0) ? r : (r * 7) / 10;
                blk(cx + pts[i][0] * rr - 1, cy + pts[i][1] * rr - 1, i % 2 == 0 ? c : dim);
            }
            if (f >= last - 1) {
                px(cx - 2, cy - 2, 4, 4, dim);  // the wisps meet
            }
            break;
        }
        case content::SkillKind::Heal: {
            // Sparks rising, a small cross on the last frame.
            const int n = 3 + s;
            for (int i = 0; i < n; ++i) {
                const int x = cx - n * 3 + i * 6 + 2;
                const int y = cy + 6 - f * 4 - (i % 2) * 3;
                blk(x, y, i % 2 == 0 ? c : dim);
            }
            if (f >= last - 1) {
                px(cx - 1, cy - 6, 2, 6, c);
                px(cx - 3, cy - 4, 6, 2, c);
            }
            break;
        }
        case content::SkillKind::Buff:
        case content::SkillKind::Debuff: {
            // A stack of chevrons rising (buff) or sinking (debuff).
            const bool up = kind == content::SkillKind::Buff;
            for (int i = 0; i < 3; ++i) {
                const int y = up ? cy + 8 - f * 3 - i * 4 : cy - 8 + f * 3 + i * 4;
                chevron(cx, y, up, i == 1 ? c : dim);
            }
            break;
        }
        case content::SkillKind::Summon: break;
    }
}

}  // namespace cd::render
