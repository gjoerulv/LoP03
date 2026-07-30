#pragma once

#include <string>
#include <vector>

// M56 — per-theme battle backdrops. A subdued theme dressing drawn behind the
// M46 battle grounding. The geometry is a pure list (headless-tested against the
// "subdued" rules); a thin raylib mapper turns roles into palette colours and
// draws them. No texture, no per-frame heap growth beyond the small vector.

namespace cd::render {

// Named BackdropStage (not BackdropStage) to avoid colliding with the
// BattleSequencer's own render::BackdropStage animation-phase enum.
enum class BackdropStage { Plain, Keep, Mine, Forest, Castle };

// Theme id -> stage. Unknown/empty -> Plain (fail-soft). Pure.
BackdropStage stageForTheme(const std::string& themeId);

// Rect roles. Ink/BorderDark are the silhouette bodies (always drawn);
// AccentCrystal/AccentMagic/AccentGold are the sparse accents dropped in high
// contrast.
enum class BackdropRole { Ink, BorderDark, AccentCrystal, AccentMagic, AccentGold };

bool isAccentRole(BackdropRole r);

struct BackdropRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    BackdropRole role = BackdropRole::BorderDark;
};

// The battle "band" the backdrop lives inside (the M46 value band behind the
// combatants).
struct BackdropBand {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// The central float/status corridor that must stay clear (as fractions of the
// band): horizontally the inner span from kCorridorXMarginPx in from each edge,
// vertically from just under the top skyline strip to the top of the bottom
// silhouette strip. Exposed so the tests assert the same rectangle the builder
// avoids.
inline constexpr int kSkylineStripPx = 8;         // top strip where skyline silhouettes sit
inline constexpr int kCorridorXMarginPx = 70;     // side columns that may hold edge silhouettes
inline constexpr float kSilhouetteFloorFrac = 0.60f;  // bottom (1 - this) of the band = floor strip
inline constexpr float kMaxCoverageFrac = 0.25f;  // <= 25% of the band may be covered

// Pure geometry: the subdued backdrop rects for `stage` inside `band`. `phase`
// is a 0/1 motion frame (one glint). `accents == false` (high contrast) drops
// every accent-role rect, keeping the ink/borderDark silhouettes. Deterministic.
std::vector<BackdropRect> buildBackdrop(BackdropStage stage, BackdropBand band, int phase,
                                        bool accents);

// raylib: build and draw the backdrop, mapping roles to palette() colours. Must
// run inside a draw pass; called from BattleState::render between the band fill
// and the ink keylines.
void drawBattleBackdrop(BackdropStage stage, BackdropBand band, int phase, bool accents);

}  // namespace cd::render
