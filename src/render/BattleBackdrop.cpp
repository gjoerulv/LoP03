#include "render/BattleBackdrop.hpp"

#include "raylib.h"
#include "ui/UiStyle.hpp"

namespace cd::render {

BackdropStage stageForTheme(const std::string& themeId) {
    if (themeId == "ruined_keep") return BackdropStage::Keep;
    if (themeId == "crystal_mine") return BackdropStage::Mine;
    if (themeId == "hollow_forest") return BackdropStage::Forest;
    return BackdropStage::Plain;  // castle passes BackdropStage::Castle explicitly
}

bool isAccentRole(BackdropRole r) {
    return r == BackdropRole::AccentCrystal || r == BackdropRole::AccentMagic ||
           r == BackdropRole::AccentGold;
}

std::vector<BackdropRect> buildBackdrop(BackdropStage stage, BackdropBand band, int phase,
                                        bool accents) {
    std::vector<BackdropRect> out;
    if (stage == BackdropStage::Plain) {
        return out;
    }
    const int skyY = band.y;                                        // top skyline strip
    const int floorTop = band.y + static_cast<int>(band.h * kSilhouetteFloorFrac);
    const int floorBase = band.y + band.h - 14;                     // silhouette baseline
    const int right = band.x + band.w;

    const auto add = [&](int x, int y, int w, int h, BackdropRole role) {
        if (w <= 0 || h <= 0) return;
        if (!accents && isAccentRole(role)) return;  // high contrast: drop accents
        out.push_back({x, y, w, h, role});
    };

    switch (stage) {
        case BackdropStage::Keep: {
            // Broken parapet merlons along the top skyline.
            for (int i = 0; i < 8; ++i) {
                const int x = band.x + 18 + i * 52;
                add(x, skyY, 16, 6, BackdropRole::BorderDark);
                add(x, skyY, 16, 1, BackdropRole::Ink);
            }
            // Tumbled-block piles at the floor (far-left, far-right, and low-centre
            // BELOW the corridor).
            const int px[3] = {band.x + 30, band.x + band.w / 2 - 14, right - 60};
            for (int p = 0; p < 3; ++p) {
                add(px[p], floorBase + 4, 30, 10, BackdropRole::BorderDark);
                add(px[p] + 6, floorBase, 18, 6, BackdropRole::BorderDark);
                add(px[p], floorBase + 4, 30, 1, BackdropRole::Ink);
            }
            break;
        }
        case BackdropStage::Mine: {
            // Staggered angular crystal clusters rising from the floor.
            const int cx[4] = {band.x + 50, band.x + band.w / 2 - 10, right - 150, right - 60};
            for (int c = 0; c < 4; ++c) {
                add(cx[c], floorBase - 6, 8, 20, BackdropRole::BorderDark);
                add(cx[c] + 8, floorBase, 6, 14, BackdropRole::BorderDark);
                add(cx[c] - 6, floorBase + 4, 6, 10, BackdropRole::BorderDark);
                add(cx[c], floorBase - 6, 8, 1, BackdropRole::Ink);
            }
            // One 1px cyan facet glint that steps 1px on the motion phase.
            add(cx[0] + 3 + (phase == 0 ? 0 : 1), floorBase - 2, 1, 4,
                BackdropRole::AccentCrystal);
            break;
        }
        case BackdropStage::Forest: {
            // Rough trunk columns with ink root flares.
            const int tx[5] = {band.x + 30, band.x + 96, band.x + band.w / 2 - 5, right - 96 - 10,
                               right - 40};
            for (int t = 0; t < 5; ++t) {
                add(tx[t], floorBase - 10, 10, 24, BackdropRole::BorderDark);
                add(tx[t] - 4, floorBase + 10, 18, 4, BackdropRole::Ink);  // root flare
            }
            // Sparse stepped canopy strip along the skyline.
            for (int i = 0; i < 6; ++i) {
                add(band.x + 10 + i * 70, skyY + 1, 40, 6, BackdropRole::BorderDark);
            }
            break;
        }
        case BackdropStage::Castle: {
            // Twin notched banners flanking, hung at the FAR edges (clear of the
            // central corridor), plus a stepped throne-arch outline at the floor.
            const int bx[2] = {band.x + 52, right - 52 - 14};
            for (int s = 0; s < 2; ++s) {
                const int x = bx[s];
                add(x, band.y + 8, 14, 42, BackdropRole::BorderDark);
                add(x, band.y + 8, 14, 1, BackdropRole::Ink);
                add(x + 3, band.y + 50, 8, 6, BackdropRole::BorderDark);  // notched tail
                add(x + 5, band.y + 18, 2, 2, BackdropRole::AccentGold);  // gold pip
            }
            // Stepped throne-arch outline (ink) below the corridor, centred.
            const int ax = band.x + band.w / 2;
            add(ax - 23, floorBase - 14, 46, 3, BackdropRole::Ink);
            add(ax - 17, floorBase - 11, 34, 3, BackdropRole::Ink);
            add(ax - 11, floorBase - 8, 22, 3, BackdropRole::Ink);
            add(ax - 23, floorBase - 4, 46, 1, BackdropRole::AccentGold);  // single gold keyline
            break;
        }
        case BackdropStage::Plain:
            break;
    }
    (void)floorTop;  // documented boundary; the builder places silhouettes below it
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
}  // namespace

void drawBattleBackdrop(BackdropStage stage, BackdropBand band, int phase, bool accents) {
    const ui::style::Palette& p = ui::style::palette();
    for (const BackdropRect& r : buildBackdrop(stage, band, phase, accents)) {
        DrawRectangle(r.x, r.y, r.w, r.h, roleColor(r.role, p));
    }
}

}  // namespace cd::render
