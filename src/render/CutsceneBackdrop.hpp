#pragma once

#include <string>

// M113 — cutscene stages. A cutscene draws a full-screen stage texture behind
// its actors instead of the M97 flat sky/floor fills: the mountainous town
// panorama for every town-triggered scene (arrivals, the finale, the roadside
// Stranger) and one of the four dungeon-theme stages for a scene triggered
// inside a dungeon (the Stranger's story rooms, the patrol scenes). The stage
// is ALWAYS caller-supplied — a scene id never implies a place (the same
// patrol tale plays in the Keep and in the Mine), so `CutsceneDef` carries no
// theme and nothing here reads one. Pure; the raylib draw lives in
// CutsceneState::render, which falls back to the old flat fills when the
// texture is missing.

namespace cd::render {

enum class CutsceneStage { Panorama, Keep, Mine, Forest, Goosy };

// Theme id -> stage. Unknown/empty -> Panorama (fail-soft).
inline CutsceneStage cutsceneStageForTheme(const std::string& themeId) {
    if (themeId == "ruined_keep") return CutsceneStage::Keep;
    if (themeId == "crystal_mine") return CutsceneStage::Mine;
    if (themeId == "hollow_forest") return CutsceneStage::Forest;
    if (themeId == "goosy_gauntlet") return CutsceneStage::Goosy;
    return CutsceneStage::Panorama;
}

// The manifest texture id of a stage (assets/manifest.json, 426x240).
inline const char* cutsceneStageTextureId(CutsceneStage stage) {
    switch (stage) {
        case CutsceneStage::Panorama: return "bg.cutscene.panorama";
        case CutsceneStage::Keep: return "bg.cutscene.keep";
        case CutsceneStage::Mine: return "bg.cutscene.mine";
        case CutsceneStage::Forest: return "bg.cutscene.forest";
        case CutsceneStage::Goosy: return "bg.cutscene.goosy";
    }
    return "bg.cutscene.panorama";
}

inline constexpr int kCutsceneStageCount = 5;

}  // namespace cd::render
