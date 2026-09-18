#pragma once

#include <string>
#include <vector>

// M56 — per-theme battle backdrops. A subdued theme dressing drawn behind the
// M46 battle grounding. The geometry is a pure list (headless-tested against the
// layer rules); a thin raylib mapper turns roles into palette colours and draws
// them. No per-frame heap growth beyond the small vectors.
//
// M119 (corrected 2026-09-16, owner brief "grounded battle stages"): the stage
// is three layers. (1) A procedural GROUND PLANE — the far strip, a horizon,
// the ground mass and its perspective seams — drawn under everything, so a
// fight always has a floor: it is the whole stage in high contrast and when a
// painting is missing. (2) The painted far/ground texture per themed stage
// (`bg.battle.<stage>`, authored at exactly the band's size and drawn
// unscaled) when accents are on. (3) The M56 ink silhouettes on top, confined
// to the skyline strip, the two margins and the near strip's open centre —
// never the ACTION FIELD where the combatants, meters, numerals, status lines
// and floats live. The field is quiet, not empty: the painting carries only
// low-contrast ground cues through it (the generator asserts that before it
// saves a stage).

namespace cd {
class ResourceManager;
}

namespace cd::render {

// Named BackdropStage (not BattleStage) to avoid colliding with the
// BattleSequencer's own render::BattleStage animation-phase enum.
// Goosy appended 2026-08-17: the Goosy Gauntlet's reed-and-pond dressing.
enum class BackdropStage { Plain, Keep, Mine, Forest, Castle, Goosy };

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

// The battle "band" the stage lives inside. BattleState draws it from y 24 to
// two pixels above the command panel — (0, 24, 426, 150) at the virtual
// resolution — so every formation row's feet and the party meters lie over
// the environment, never below it.
struct BackdropBand {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

// The painted stages are authored at exactly the band's size and drawn
// unscaled at its origin (a wrong-sized texture would simply not cover the
// band — it is never stretched); a test pins the shipped PNGs to this.
inline constexpr int kStageTextureW = 426;
inline constexpr int kStageTextureH = 150;

// The stage's zones, measured from the band's edges (screen space = band
// space + the band's origin):
//  - the skyline strip: the top rows, where far-layer silhouettes sit
//    (merlons, canopy, reed tops);
//  - the horizon: where the far layer ends and the ground plane begins —
//    every normal formation row's feet lie on or below it (a five-foe field's
//    top row stands exactly on it);
//  - the near strip: the bottom rows, where near-edge silhouettes sit;
//  - the left margin: the free column left of the enemy sprites (x 36+);
//  - the right margin: the free column right of the party numerals;
//  - the near strip's open centre: between the enemy status column and the
//    party column, where a low-centre silhouette may sit.
inline constexpr int kSkylineStripPx = 8;
inline constexpr int kHorizonPx = 12;
inline constexpr int kNearStripPx = 10;
inline constexpr int kLeftMarginPx = 30;
inline constexpr int kRightMarginPx = 18;
inline constexpr int kNearCentreX0 = 90;
inline constexpr int kNearCentreX1 = 290;
inline constexpr float kMaxCoverageFrac = 0.25f;  // <= 25% of the band may hold silhouettes

// The action field: the band minus the skyline strip, the near strip and the
// two margins — everything the combatants, meters, numerals, status lines
// and floats can touch. Silhouette rects never enter it; the painting may
// carry only quiet ground cues inside it. Pure.
BackdropBand actionField(BackdropBand band);

// True when a silhouette rect lies inside the band and entirely inside one
// allowed zone: the skyline strip, the left margin, the right margin, or the
// near strip's open centre. Pure; the builder's contract and the tests' pin.
bool silhouetteAllowed(const BackdropRect& r, BackdropBand band);

// Pure geometry: the silhouette rects for `stage` inside `band`. `phase` is
// a 0/1 motion frame (one glint). `accents == false` (high contrast) drops
// every accent-role rect, keeping the ink/borderDark bodies. Deterministic.
// Plain has none.
std::vector<BackdropRect> buildBackdrop(BackdropStage stage, BackdropBand band, int phase,
                                        bool accents);

// The procedural ground plane: the far strip above the horizon, the horizon
// line, the ground mass below it, and 1 px perspective seams whose spacing
// grows toward the near edge. Stage-independent and accent-free (shape and
// value only, so high contrast keeps it whole). Pure.
enum class GroundRole { Far, Horizon, Ground, Seam };

struct GroundRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    GroundRole role = GroundRole::Ground;
};

std::vector<GroundRect> buildGroundPlane(BackdropBand band);

// raylib: build and draw the ground plane / the silhouettes, mapping roles to
// palette() colours. Must run inside a draw pass.
void drawGroundPlane(BackdropBand band);
void drawBattleBackdrop(BackdropStage stage, BackdropBand band, int phase, bool accents);

// The painted stage's texture id (`bg.battle.<stage>`); Plain has none.
const char* battleStageTextureId(BackdropStage stage);

// The whole stage, in order: the ground plane, the painted texture (accents
// on and the texture present — unscaled at the band's origin), then the
// silhouettes. Called from BattleState::render between the band fill and the
// ink keylines.
void drawBattleStage(ResourceManager& resources, BackdropStage stage, BackdropBand band,
                     int phase, bool accents);

}  // namespace cd::render
