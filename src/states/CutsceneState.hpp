#pragma once

#include <string>

#include "content/Definitions.hpp"
#include "render/CutsceneBackdrop.hpp"  // M113
#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/TextViewport.hpp"

namespace cd {

struct AppContext;

// M97: a Hooded Goose story scene — a full-screen stage (party standing,
// the hooded goose center, King/Dragon staged when a beat asks) over a
// bottom dialogue panel, ending in the mandatory pick-one-of-two heirloom
// choice. Cancel during the beats offers "Skip scene?" which jumps TO the
// choice — the choice itself is never skippable. The grant fires only while
// the party has no recorded choice for the scene (game/Cutscenes.hpp), so
// finale replays are safe; `replay` additionally suppresses it outright
// (the debug menu's no-re-grant play). Trigger sites mark the scene seen
// BEFORE pushing; this state never touches the seen list.
class CutsceneState : public GameState {
public:
    // M113: `stage` is the backdrop the CALLER knows — the town panorama
    // (default) or the dungeon theme's stage; a scene id never implies one.
    CutsceneState(StateStack& stack, AppContext& context, std::string sceneId, bool replay,
                  render::CutsceneStage stage = render::CutsceneStage::Panorama);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    void captureShowBeat(int index);  // park on a beat (deterministic stage)
    void captureShowChoice();         // park on the choice modal
#endif

private:
    enum class Phase { Beats, SkipAsk, Choice, Response };

    void loadBeat();
    void advanceBeat();
    void enterChoice();
    void commitChoice();
    const content::CutsceneBeat* currentBeat() const;

    AppContext& context_;
    std::string sceneId_;
    bool replay_ = false;
    render::CutsceneStage stage_ = render::CutsceneStage::Panorama;  // M113
    const content::CutsceneDef* def_ = nullptr;  // null = unauthored; pops on first input
    Phase phase_ = Phase::Beats;
    int beatIndex_ = 0;
    int chosen_ = -1;  // options index once committed
    // Wrapped lines the offer box reserves for the highlighted keepsake's
    // description — the max over all options, measured in enterChoice, so
    // every heirloom's own words fit (owner fix 2026-08-17).
    int choiceDetailLines_ = 2;
    ui::TextViewport bodyView_;
    ui::Menu choiceMenu_;
    std::string panelSpeaker_;  // resolved speaker of the shown line
    std::string panelText_;     // resolved text of the shown line
};

}  // namespace cd
