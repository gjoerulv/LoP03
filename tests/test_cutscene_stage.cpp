// M113 - cutscene stages: the theme -> stage mapping, the texture ids, and
// the shipped manifest carrying every stage.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "assets/AssetManifest.hpp"
#include "content/LoadReport.hpp"
#include "render/CutsceneBackdrop.hpp"

using cd::render::CutsceneStage;

TEST_CASE("cutscene stage: every dungeon theme maps to its own stage; anything else is the panorama",
          "[cutscene][stage]") {
    CHECK(cd::render::cutsceneStageForTheme("ruined_keep") == CutsceneStage::Keep);
    CHECK(cd::render::cutsceneStageForTheme("crystal_mine") == CutsceneStage::Mine);
    CHECK(cd::render::cutsceneStageForTheme("hollow_forest") == CutsceneStage::Forest);
    CHECK(cd::render::cutsceneStageForTheme("goosy_gauntlet") == CutsceneStage::Goosy);
    CHECK(cd::render::cutsceneStageForTheme("") == CutsceneStage::Panorama);
    CHECK(cd::render::cutsceneStageForTheme("castle") == CutsceneStage::Panorama);
    CHECK(cd::render::cutsceneStageForTheme("no_such_theme") == CutsceneStage::Panorama);
}

TEST_CASE("cutscene stage: five distinct texture ids, all in the shipped manifest",
          "[cutscene][stage][lint]") {
    const CutsceneStage stages[] = {CutsceneStage::Panorama, CutsceneStage::Keep,
                                    CutsceneStage::Mine, CutsceneStage::Forest,
                                    CutsceneStage::Goosy};
    std::set<std::string> ids;
    for (const CutsceneStage s : stages) {
        ids.insert(cd::render::cutsceneStageTextureId(s));
    }
    CHECK(ids.size() == static_cast<std::size_t>(cd::render::kCutsceneStageCount));

    cd::assets::AssetManifest m;
    cd::content::LoadReport report;
    REQUIRE(m.load(std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR), report));
    for (const std::string& id : ids) {
        INFO(id);
        const cd::assets::AssetEntry* e = m.find(id);
        REQUIRE(e != nullptr);
        CHECK(e->type == cd::assets::AssetType::Texture);
    }
}
