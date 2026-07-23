#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The Settings screen (M13; reorganized into submenus M51): a top-level menu
// (Audio / Display / Gameplay / Controls / Reset / Back) each opening a focused
// sub-list. Changes apply immediately and persist to settings.json when the
// screen closes (Cancel at the top level), and on reset. Cancel inside a submenu
// steps back to the top level.
class SettingsState : public GameState {
public:
    SettingsState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void onResume() override;
    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M51): open the Display submenu so its rows (incl. CRT Effect)
    // are captured.
    void captureShowDisplay();
    // Capture-only (M52): open the Audio submenu so its rows (incl. the new
    // Ambience Volume row) are overflow-checked.
    void captureShowAudio();
#endif

private:
    // M51: which list is showing. Top is the category picker; the rest are the
    // focused option lists.
    enum class Mode { Top, Audio, Display, Gameplay, Controls };

    // One row of the current mode: what it does when adjusted (Left/Right) or
    // activated (Confirm). `kind` selects the behaviour in adjust()/activate().
    enum class Row {
        // Top-level category entries (Confirm opens the submenu).
        CatAudio, CatDisplay, CatGameplay, CatControls, ResetAll, Back,
        // Audio.
        MasterVolume, MusicVolume, SfxVolume, AmbienceVolume, BackgroundAudio,
        // Display.
        Window, CrtEffect, BattleFlash, BattleShake, HighContrast,
        // Gameplay.
        BattleSpeed, MessageSpeed, TutorialPrompts, ResetTutorial,
        // Controls.
        RemapKeyboard, RemapGamepad,
    };

    void rebuild();
    void adjust(Row row, int direction);
    void activate(Row row);
    void applyAudio();
    void saveSettings();
    Row rowAt(int index) const;  // the Row for the current cursor position

    AppContext& context_;
    Mode mode_ = Mode::Top;
    ui::Menu menu_;
    std::vector<Row> rows_;  // parallel to menu_ items, per mode
    std::string message_;
};

}  // namespace cd
