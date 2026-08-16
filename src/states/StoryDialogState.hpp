#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/TextViewport.hpp"

namespace cd {

struct AppContext;

// A story dialog overlay (M41; bounded since M87): dims and freezes the scene
// below (renders, does not update), shows the speaker's titled panel with a
// height-capped scrollable body, and dismisses on any Confirm/Cancel. A
// near-clone of TutorialPromptState with a speaker footer instead of the
// tutorial footer.
class StoryDialogState : public GameState {
public:
    StoryDialogState(StateStack& stack, AppContext& context, std::string speaker,
                     std::string title, std::string body);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }

private:
    AppContext& context_;
    std::string speaker_;
    std::string title_;
    std::string body_;
    ui::TextViewport bodyView_;
};

}  // namespace cd
