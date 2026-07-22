#pragma once

#include <functional>
#include <string>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// Transparent Yes/No confirmation: dims and freezes the scene below, states the
// consequence, and offers an explicit choice. The cursor starts on the "no" entry
// and Cancel answers no, so a destructive action always takes a deliberate press
// — the M22 "confirm twice on the same control" pattern reads as a dead key press
// when the warning is only a line of text, so screens that can lose progress use
// this instead.
class ConfirmPromptState : public GameState {
public:
    ConfirmPromptState(StateStack& stack, AppContext& context, std::string title,
                       std::string body, std::string confirmLabel, std::string cancelLabel,
                       std::function<void()> onConfirm);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }
    bool updatesBelow() const override { return false; }

private:
    void answerNo();

    AppContext& context_;
    std::string title_;
    std::string body_;
    ui::Menu menu_;
    std::function<void()> onConfirm_;
};

}  // namespace cd
