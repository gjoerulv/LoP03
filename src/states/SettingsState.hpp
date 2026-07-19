#pragma once

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The Settings screen (M13): volumes, window mode, battle/message speed,
// entry points to per-device remapping, and reset. Changes apply immediately
// and persist to settings.json when the screen closes (and on reset).
class SettingsState : public GameState {
public:
    SettingsState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void onResume() override;
    void handleInput(const Input& input) override;
    void render() override;

private:
    void rebuild();
    void adjust(int row, int direction);
    void activate(int row);
    void applyAudio();
    void saveSettings();

    AppContext& context_;
    ui::Menu menu_;
    std::string message_;
};

}  // namespace cd
