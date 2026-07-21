#pragma once

#include <string>

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// A story dialog overlay (M41): dims and freezes the scene below (renders, does
// not update), shows the speaker's titled wrapped panel, and dismisses on any
// Confirm/Cancel. Reuses the M12 wrapped-text helpers; a near-clone of
// TutorialPromptState with a speaker footer instead of the tutorial footer.
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
};

}  // namespace cd
