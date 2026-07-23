#pragma once

#include <functional>
#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// Transparent confirmation prompt: dims and freezes the scene below, states the
// consequence, and offers an explicit choice. The cursor starts on the safe entry
// and Cancel picks it too, so a destructive action always takes a deliberate press
// — the M22 "confirm twice on the same control" pattern reads as a dead key press
// when the warning is only a line of text, so screens that can lose progress use
// this instead.
//
// M47: generalized from Yes/No to N answers (the pause menus' Quit to Title /
// Quit Game / Keep Playing). The two-answer constructor delegates to the general
// one, so every pre-M47 caller is unchanged.
class ConfirmPromptState : public GameState {
public:
    // One row of the prompt. A null `onChoose` simply dismisses (the safe row).
    struct Answer {
        std::string label;
        std::function<void()> onChoose;
    };

    // General form: `safeIndex` is the row the cursor starts on and the row
    // Cancel resolves to — it must never be a destructive one.
    ConfirmPromptState(StateStack& stack, AppContext& context, std::string title,
                       std::string body, std::vector<Answer> answers, int safeIndex);

    // Yes/No form (pre-M47 callers): confirm first, cancel second, cursor on
    // cancel.
    ConfirmPromptState(StateStack& stack, AppContext& context, std::string title,
                       std::string body, std::string confirmLabel, std::string cancelLabel,
                       std::function<void()> onConfirm);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }
    bool updatesBelow() const override { return false; }

private:
    void answerSafe();

    AppContext& context_;
    std::string title_;
    std::string body_;
    ui::Menu menu_;
    std::vector<Answer> answers_;
    int safeIndex_ = 0;
};

}  // namespace cd
