#include "render/BattleBackdrop.hpp"

#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "ui/UiStyle.hpp"

namespace cd::render {

BackdropStage stageForTheme(const std::string& themeId) {
    if (themeId == "ruined_keep") return BackdropStage::Keep;
    if (themeId == "crystal_mine") return BackdropStage::Mine;
    if (themeId == "hollow_forest") return BackdropStage::Forest;
    if (themeId == "goosy_gauntlet") return BackdropStage::Goosy;
    return BackdropStage::Plain;  // castle passes BackdropStage::Castle explicitly
}

bool isAccentRole(BackdropRole r) {
    return r == BackdropRole::AccentCrystal || r == BackdropRole::AccentMagic ||
           r == BackdropRole::AccentGold;
}

BackdropBand actionField(BackdropBand band) {
    return {band.x + kLeftMarginPx, band.y + kSkylineStripPx,
            band.w - kLeftMarginPx - kRightMarginPx, band.h - kSkylineStripPx - kNearStripPx};
}

bool silhouetteAllowed(const BackdropRect& r, BackdropBand band) {
    if (r.w <= 0 || r.h <= 0) {
        return false;
    }
    const int x0 = r.x;
    const int y0 = r.y;
    const int x1 = r.x + r.w;
    const int y1 = r.y + r.h;
    const int right = band.x + band.w;
    const int bottom = band.y + band.h;
    if (x0 < band.x || y0 < band.y || x1 > right || y1 > bottom) {
        return false;  // in-band, always
    }
    if (y1 <= band.y + kSkylineStripPx) {
        return true;  // the skyline strip
    }
    if (x1 <= band.x + kLeftMarginPx) {
        return true;  // the left margin
    }
    if (x0 >= right - kRightMarginPx) {
        return true;  // the right margin
    }
    return y0 >= bottom - kNearStripPx && x0 >= band.x + kNearCentreX0 &&
           x1 <= band.x + kNearCentreX1;  // the near strip's open centre
}

std::vector<BackdropRect> buildBackdrop(BackdropStage stage, BackdropBand band, int phase,
                                        bool accents) {
    std::vector<BackdropRect> out;
    if (stage == BackdropStage::Plain) {
        return out;
    }
    const int skyY = band.y;                              // top skyline strip
    const int right = band.x + band.w;
    const int bottom = band.y + band.h;
    const int nearTop = bottom - kNearStripPx;            // the near strip's top row
    const int leftX = band.x;                             // the left margin's first column
    const int rightX = right - kRightMarginPx;            // the right margin's first column
    const int centreX = band.x + band.w / 2;

    const auto add = [&](int x, int y, int w, int h, BackdropRole role) {
        if (w <= 0 || h <= 0) return;
        if (!accents && isAccentRole(role)) return;  // high contrast: drop accents
        out.push_back({x, y, w, h, role});
    };

    switch (stage) {
        case BackdropStage::Keep: {
            // Broken parapet merlons along the top skyline, the gaps falling
            // where the two sprite columns rise so no merlon edge cuts a
            // top-row crown.
            for (int x0 : {6, 96, 148, 200, 252, 356, 404}) {
                const int x = band.x + x0;
                add(x, skyY, 16, 6, BackdropRole::BorderDark);
                add(x, skyY, 16, 1, BackdropRole::Ink);
            }
            // Tumbled-block piles at the near edge: the left margin, the open
            // centre of the near strip, the right margin.
            add(leftX + 2, nearTop + 2, 26, 8, BackdropRole::BorderDark);
            add(leftX + 8, nearTop - 2, 14, 4, BackdropRole::BorderDark);
            add(leftX + 2, nearTop + 2, 26, 1, BackdropRole::Ink);
            add(band.x + 196, nearTop + 3, 30, 7, BackdropRole::BorderDark);
            add(band.x + 202, nearTop, 18, 3, BackdropRole::BorderDark);
            add(band.x + 196, nearTop + 3, 30, 1, BackdropRole::Ink);
            add(rightX + 2, nearTop + 3, 16, 7, BackdropRole::BorderDark);
            add(rightX + 6, nearTop, 10, 3, BackdropRole::BorderDark);
            add(rightX + 2, nearTop + 3, 16, 1, BackdropRole::Ink);
            break;
        }
        case BackdropStage::Mine: {
            // Angular crystal clusters rising in the margins and one low in the
            // near strip's centre; one 1px cyan facet glint steps on the phase.
            add(leftX + 8, nearTop - 14, 8, 24, BackdropRole::BorderDark);
            add(leftX + 16, nearTop - 6, 6, 16, BackdropRole::BorderDark);
            add(leftX + 2, nearTop - 2, 6, 12, BackdropRole::BorderDark);
            add(leftX + 8, nearTop - 14, 8, 1, BackdropRole::Ink);
            add(leftX + 11 + (phase == 0 ? 0 : 1), nearTop - 10, 1, 4,
                BackdropRole::AccentCrystal);
            add(rightX + 4, nearTop - 8, 6, 18, BackdropRole::BorderDark);
            add(rightX + 10, nearTop - 2, 6, 12, BackdropRole::BorderDark);
            add(rightX + 4, nearTop - 8, 6, 1, BackdropRole::Ink);
            add(band.x + 230, nearTop + 1, 6, 9, BackdropRole::BorderDark);
            add(band.x + 236, nearTop, 8, 10, BackdropRole::BorderDark);
            add(band.x + 236, nearTop, 8, 1, BackdropRole::Ink);
            break;
        }
        case BackdropStage::Forest: {
            // Sparse stepped canopy strip along the skyline, broken over the
            // two sprite columns.
            const int canopy[7][2] = {{0, 34}, {84, 40}, {140, 40}, {200, 40},
                                      {256, 40}, {350, 40}, {400, 26}};
            for (const auto& c : canopy) {
                add(band.x + c[0], skyY + 1, c[1], 6, BackdropRole::BorderDark);
            }
            // Rough trunk columns in the margins (two left, one right) with ink
            // root flares at the near edge, and a fallen bough low in the centre.
            const int trunkY = skyY + kSkylineStripPx;
            const int trunkH = band.h - kSkylineStripPx;
            add(leftX + 4, trunkY, 10, trunkH, BackdropRole::BorderDark);
            add(leftX + 20, trunkY + 36, 6, trunkH - 36, BackdropRole::BorderDark);
            add(leftX, bottom - 6, 22, 4, BackdropRole::Ink);
            add(leftX + 16, bottom - 4, 14, 3, BackdropRole::Ink);
            add(rightX + 4, trunkY, 10, trunkH, BackdropRole::BorderDark);
            add(rightX, bottom - 6, 18, 4, BackdropRole::Ink);
            add(band.x + 120, nearTop + 4, 70, 3, BackdropRole::BorderDark);
            add(band.x + 120, nearTop + 4, 70, 1, BackdropRole::Ink);
            break;
        }
        case BackdropStage::Goosy: {
            // Broken reed-top strip along the skyline (thinner and gappier
            // than the forest canopy, so the two differ in grayscale).
            for (int x0 : {6, 88, 130, 172, 214, 256, 352, 396}) {
                add(band.x + x0, skyY + 1, 20, 5, BackdropRole::BorderDark);
            }
            // Cattail clumps in the margins: thin stalk, dark head, one offset
            // leaf blade.
            const int gx[3] = {leftX + 8, leftX + 20, rightX + 6};
            for (int c = 0; c < 3; ++c) {
                add(gx[c], nearTop - 40, 2, 50, BackdropRole::BorderDark);
                add(gx[c] - 1, nearTop - 46, 4, 8, BackdropRole::Ink);
                add(gx[c] + 4, nearTop - 22, 2, 32, BackdropRole::BorderDark);
            }
            // A low reed tussock in the near strip's centre, and one drifting
            // ripple glint beside it that steps on the motion phase.
            for (int k = 0; k < 3; ++k) {
                add(band.x + 176 + k * 6, nearTop + 2, 2, 8, BackdropRole::BorderDark);
                add(band.x + 175 + k * 6, nearTop, 4, 3, BackdropRole::Ink);
            }
            add(band.x + 200 + (phase == 0 ? 0 : 2), nearTop + 6, 4, 1,
                BackdropRole::AccentCrystal);
            break;
        }
        case BackdropStage::Castle: {
            // Twin notched banners hung in the margins (a gold pip each), plus a
            // stepped throne-arch outline low in the near strip's centre with a
            // single gold keyline.
            const int bx[2] = {leftX + 8, rightX + 2};
            for (int s = 0; s < 2; ++s) {
                const int x = bx[s];
                add(x, band.y + 8, 14, 42, BackdropRole::BorderDark);
                add(x, band.y + 8, 14, 1, BackdropRole::Ink);
                add(x + 3, band.y + 50, 8, 6, BackdropRole::BorderDark);  // notched tail
                add(x + 5, band.y + 18, 2, 2, BackdropRole::AccentGold);  // gold pip
            }
            add(centreX - 23, nearTop, 46, 2, BackdropRole::Ink);
            add(centreX - 17, nearTop + 2, 34, 2, BackdropRole::Ink);
            add(centreX - 11, nearTop + 4, 22, 2, BackdropRole::Ink);
            add(centreX - 23, nearTop + 8, 46, 1, BackdropRole::AccentGold);  // single gold keyline
            break;
        }
        case BackdropStage::Plain:
            break;
    }
    return out;
}

std::vector<GroundRect> buildGroundPlane(BackdropBand band) {
    std::vector<GroundRect> out;
    const int horizon = band.y + kHorizonPx;
    const int bottom = band.y + band.h;
    out.push_back({band.x, band.y, band.w, kHorizonPx, GroundRole::Far});
    out.push_back({band.x, horizon, band.w, bottom - horizon, GroundRole::Ground});
    out.push_back({band.x, horizon, band.w, 1, GroundRole::Horizon});
    // Perspective seams: 1px courses whose spacing grows toward the near edge
    // (12, 17, 22, ... px), stopping above the near strip.
    for (int y = horizon, gap = 12; y + gap < bottom - kNearStripPx; gap += 5) {
        y += gap;
        out.push_back({band.x, y, band.w, 1, GroundRole::Seam});
    }
    return out;
}

namespace {
Color roleColor(BackdropRole role, const ui::style::Palette& p) {
    switch (role) {
        case BackdropRole::Ink: return p.ink;
        case BackdropRole::BorderDark: return p.borderDark;
        case BackdropRole::AccentCrystal: return p.crystal;
        case BackdropRole::AccentMagic: return p.magic;
        case BackdropRole::AccentGold: return p.gold;
    }
    return p.borderDark;
}

// Value steps only — the plane must read in grayscale and in high contrast:
// the far strip darkest, the ground a step lighter, the horizon a lit edge
// between them, the seams a touch darker than the ground.
Color groundColor(GroundRole role, const ui::style::Palette& p) {
    switch (role) {
        case GroundRole::Far: return p.panelInset;
        case GroundRole::Horizon: return p.borderDark;
        case GroundRole::Ground: return p.panelRaised;
        case GroundRole::Seam: return p.panel;
    }
    return p.panel;
}
}  // namespace

void drawGroundPlane(BackdropBand band) {
    const ui::style::Palette& p = ui::style::palette();
    for (const GroundRect& r : buildGroundPlane(band)) {
        DrawRectangle(r.x, r.y, r.w, r.h, groundColor(r.role, p));
    }
}

void drawBattleBackdrop(BackdropStage stage, BackdropBand band, int phase, bool accents) {
    const ui::style::Palette& p = ui::style::palette();
    for (const BackdropRect& r : buildBackdrop(stage, band, phase, accents)) {
        DrawRectangle(r.x, r.y, r.w, r.h, roleColor(r.role, p));
    }
}

const char* battleStageTextureId(BackdropStage stage) {
    switch (stage) {
        case BackdropStage::Keep: return "bg.battle.keep";
        case BackdropStage::Mine: return "bg.battle.mine";
        case BackdropStage::Forest: return "bg.battle.forest";
        case BackdropStage::Castle: return "bg.battle.castle";
        case BackdropStage::Goosy: return "bg.battle.goosy";
        case BackdropStage::Plain: break;
    }
    return nullptr;
}

void drawBattleStage(ResourceManager& resources, BackdropStage stage, BackdropBand band,
                     int phase, bool accents) {
    drawGroundPlane(band);
    if (accents) {
        if (const char* id = battleStageTextureId(stage);
            id != nullptr && resources.hasTexture(id)) {
            // Unscaled at the band's origin: the painting is authored at the
            // band's size (kStageTextureW x kStageTextureH), never stretched.
            DrawTexture(resources.texture(id), band.x, band.y, WHITE);
        }
    }
    drawBattleBackdrop(stage, band, phase, accents);
}

}  // namespace cd::render
