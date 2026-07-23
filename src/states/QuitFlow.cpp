#include "states/QuitFlow.hpp"

#include <memory>
#include <vector>

#include "states/ConfirmPromptState.hpp"
#include "states/MainMenuState.hpp"
#include "states/QuitPrompt.hpp"
#include "states/StateStack.hpp"

namespace cd {

void pushQuitPrompt(StateStack& stack, AppContext& context, const char* body) {
    StateStack* stackPtr = &stack;
    AppContext* contextPtr = &context;
    const std::array<const char*, quit::kAnswerCount> labels = quit::answerLabels();

    std::vector<ConfirmPromptState::Answer> answers;
    answers.reserve(quit::kAnswerCount);
    // Quit to Title: drop the whole stack and start the menu over — the pre-M47
    // behavior, unchanged.
    answers.push_back({labels[quit::kQuitToTitle], [stackPtr, contextPtr]() {
                           stackPtr->clearStates();
                           stackPtr->pushState(
                               std::make_unique<MainMenuState>(*stackPtr, *contextPtr));
                       }});
    // Quit Game: clear the stack and push NOTHING. Application::run's
    // `!stack_.empty()` guard then ends the loop cleanly on the next frame —
    // no exit flag, no forced window close. Settings persist on change, so
    // there is nothing left to flush.
    answers.push_back({labels[quit::kQuitGame], [stackPtr]() { stackPtr->clearStates(); }});
    // Keep Playing: no action; the prompt simply dismisses.
    answers.push_back({labels[quit::kKeepPlaying], nullptr});

    stack.pushState(std::make_unique<ConfirmPromptState>(stack, context, quit::kTitle, body,
                                                         std::move(answers), quit::kSafeAnswer));
}

}  // namespace cd
