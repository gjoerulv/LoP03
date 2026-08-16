#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/TextViewport.hpp"

namespace cd {

struct AppContext;

// Transparent one-time tutorial prompt (M22; bounded since M87): dims and
// freezes the scene below (renders, does not update), shows a titled panel
// with a height-capped scrollable body, and any Confirm/Cancel press
// dismisses it. The beat is already marked seen by
// tutorial::TutorialStore::takeBeat before this state is pushed.
class TutorialPromptState : public GameState {
public:
    TutorialPromptState(StateStack& stack, AppContext& context, std::string title,
                        std::string body);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }

private:
    AppContext& context_;
    std::string title_;
    std::string body_;
    ui::TextViewport bodyView_;
};

// Fires the beat's prompt if it has not been seen and prompts are enabled;
// call from a state's onEnter/onResume/update (never from a constructor —
// queued pushes would put the prompt underneath the state being created).
void maybeTutorialPrompt(StateStack& stack, AppContext& context, const char* beatId);

}  // namespace cd
