#include "states/SettingsState.hpp"

#include <memory>
#include <string>
#include <vector>

#include "audio/AudioManager.hpp"
#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "core/Log.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "input/Remap.hpp"
#include "raylib.h"
#include "settings/Settings.hpp"
#include "states/RemapState.hpp"
#include "states/StateStack.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
int volumeSteps(float v) { return static_cast<int>(v * 10.0f + 0.5f); }

std::string volumeLabel(const char* name, float v) {
    return std::string(name) + ":  < " + std::to_string(volumeSteps(v)) + " >";
}

std::string toggleLabel(const char* name, bool on) {
    return std::string(name) + ":  < " + (on ? "On" : "Off") + " >";
}
}  // namespace

SettingsState::SettingsState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void SettingsState::onEnter() { rebuild(); }
void SettingsState::onResume() { rebuild(); }

#ifdef CRYSTAL_CAPTURE
void SettingsState::captureShowDisplay() {
    mode_ = Mode::Display;
    menu_.setCursor(0);
    rebuild();
}
#endif

SettingsState::Row SettingsState::rowAt(int index) const {
    if (index < 0 || index >= static_cast<int>(rows_.size())) {
        return Row::Back;
    }
    return rows_[static_cast<std::size_t>(index)];
}

void SettingsState::rebuild() {
    const settings::Settings& v = context_.settings.values;
    const bool tutorialsOn = context_.tutorial.state.enabled;
    std::vector<ui::MenuItem> items;
    rows_.clear();
    const auto add = [&](std::string label, Row row, bool enabled = true) {
        items.push_back({std::move(label), enabled});
        rows_.push_back(row);
    };

    switch (mode_) {
        case Mode::Top:
            add("Audio...", Row::CatAudio);
            add("Display...", Row::CatDisplay);
            add("Gameplay...", Row::CatGameplay);
            add("Controls...", Row::CatControls);
            add("Reset settings and bindings", Row::ResetAll);
            add("Back", Row::Back);
            break;
        case Mode::Audio:
            add(volumeLabel("Master Volume", v.masterVolume), Row::MasterVolume);
            add(volumeLabel("Music Volume", v.musicVolume), Row::MusicVolume);
            add(volumeLabel("SFX Volume", v.sfxVolume), Row::SfxVolume);
            add(toggleLabel("Background Audio", v.backgroundAudio), Row::BackgroundAudio);
            add("Back", Row::Back);
            break;
        case Mode::Display:
            add(std::string("Window:  < ") + (v.borderlessFullscreen ? "Borderless" : "Windowed") +
                    " >",
                Row::Window);
            add(toggleLabel("CRT Effect", v.crtEffect), Row::CrtEffect);
            add(std::string("Battle Flash:  < ") +
                    std::string(settings::effectLevelName(v.effectFlash)) + " >",
                Row::BattleFlash);
            add(std::string("Battle Shake:  < ") +
                    std::string(settings::effectLevelName(v.effectShake)) + " >",
                Row::BattleShake);
            add(toggleLabel("High Contrast", v.highContrast), Row::HighContrast);
            add("Back", Row::Back);
            break;
        case Mode::Gameplay:
            add(std::string("Battle Speed:  < ") +
                    std::string(settings::battleSpeedName(v.battleSpeed)) + " >",
                Row::BattleSpeed);
            add(std::string("Message Speed:  < ") +
                    std::string(settings::messageSpeedName(v.messageSpeed)) + " >",
                Row::MessageSpeed);
            add(toggleLabel("Tutorial Prompts", tutorialsOn), Row::TutorialPrompts);
            add("Reset tutorial prompts", Row::ResetTutorial);
            add("Back", Row::Back);
            break;
        case Mode::Controls:
            add("Remap Keyboard...", Row::RemapKeyboard);
            add("Remap Gamepad...", Row::RemapGamepad, context_.input.gamepadAvailable());
            add("Back", Row::Back);
            break;
    }

    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void SettingsState::applyAudio() {
    const settings::Settings& v = context_.settings.values;
    context_.audio.setVolumes(v.masterVolume, v.musicVolume, v.sfxVolume);
}

void SettingsState::saveSettings() {
    content::LoadReport report;
    if (!context_.settings.save(context_.input.map(), report)) {
        for (const auto& e : report.errors()) {
            log::warn(e.source + ": " + e.context + ": " + e.message);
        }
        message_ = "Could not save settings (see log)";
    }
}

void SettingsState::adjust(Row row, int direction) {
    settings::Settings& v = context_.settings.values;
    const auto stepVolume = [direction](float value) {
        float next = value + 0.1f * static_cast<float>(direction);
        return next < 0.0f ? 0.0f : (next > 1.0f ? 1.0f : next);
    };
    const auto cycle3 = [direction](int s) { return (s + direction) % 3 < 0 ? (s + direction + 3) % 3
                                                                            : (s + direction) % 3; };
    switch (row) {
        case Row::MasterVolume: v.masterVolume = stepVolume(v.masterVolume); applyAudio(); break;
        case Row::MusicVolume: v.musicVolume = stepVolume(v.musicVolume); applyAudio(); break;
        case Row::SfxVolume: v.sfxVolume = stepVolume(v.sfxVolume); applyAudio(); break;
        case Row::BackgroundAudio: v.backgroundAudio = !v.backgroundAudio; break;
        case Row::Window: v.borderlessFullscreen = !v.borderlessFullscreen; break;
        case Row::CrtEffect: v.crtEffect = !v.crtEffect; break;  // Application reads it each frame
        case Row::BattleFlash:
            v.effectFlash = static_cast<settings::EffectLevel>(cycle3(static_cast<int>(v.effectFlash)));
            break;
        case Row::BattleShake:
            v.effectShake = static_cast<settings::EffectLevel>(cycle3(static_cast<int>(v.effectShake)));
            break;
        case Row::HighContrast:
            v.highContrast = !v.highContrast;
            ui::style::setHighContrast(v.highContrast);  // visible immediately
            break;
        case Row::BattleSpeed:
            v.battleSpeed = static_cast<settings::BattleSpeed>(cycle3(static_cast<int>(v.battleSpeed)));
            break;
        case Row::MessageSpeed:
            v.messageSpeed =
                static_cast<settings::MessageSpeed>(cycle3(static_cast<int>(v.messageSpeed)));
            break;
        case Row::TutorialPrompts: {
            context_.tutorial.state.enabled = !context_.tutorial.state.enabled;
            content::LoadReport report;
            context_.tutorial.save(report);
            break;
        }
        default: return;  // non-adjustable rows
    }
    context_.audio.play(Sfx::Move);
    rebuild();
}

void SettingsState::activate(Row row) {
    switch (row) {
        case Row::CatAudio: mode_ = Mode::Audio; menu_.setCursor(0); rebuild(); break;
        case Row::CatDisplay: mode_ = Mode::Display; menu_.setCursor(0); rebuild(); break;
        case Row::CatGameplay: mode_ = Mode::Gameplay; menu_.setCursor(0); rebuild(); break;
        case Row::CatControls: mode_ = Mode::Controls; menu_.setCursor(0); rebuild(); break;
        case Row::RemapKeyboard:
            stack().pushState(
                std::make_unique<RemapState>(stack(), context_, ActiveDevice::Keyboard));
            break;
        case Row::RemapGamepad:
            stack().pushState(
                std::make_unique<RemapState>(stack(), context_, ActiveDevice::Gamepad));
            break;
        case Row::ResetTutorial:
            context_.tutorial.reset();
            message_ = "Tutorial prompts will show again";
            rebuild();
            break;
        case Row::ResetAll:
            context_.settings.values = settings::Settings{};
            input::resetBindings(context_.input.map());
            applyAudio();
            ui::style::setHighContrast(context_.settings.values.highContrast);
            saveSettings();
            message_ = "Settings and bindings reset to defaults";
            rebuild();
            break;
        case Row::Back:
            if (mode_ == Mode::Top) {
                saveSettings();
                stack().popState();
            } else {
                mode_ = Mode::Top;
                menu_.setCursor(0);
                rebuild();
            }
            break;
        default:
            break;  // adjustable rows do nothing on Confirm
    }
}

void SettingsState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        context_.audio.play(Sfx::Move);
    }
    const int dir = (input.navPressed(InputAction::MoveRight) ? 1 : 0) -
                    (input.navPressed(InputAction::MoveLeft) ? 1 : 0);
    if (dir != 0) {
        adjust(rowAt(menu_.cursor()), dir);
    }
    if (input.pressed(InputAction::Confirm) && menu_.currentEnabled()) {
        context_.audio.play(Sfx::Confirm);
        activate(rowAt(menu_.cursor()));
    }
    if (input.pressed(InputAction::Cancel)) {
        // Cancel steps out one level: a submenu returns to the top, the top saves
        // and closes (the pre-M51 behaviour).
        context_.audio.play(Sfx::Cancel);
        if (mode_ == Mode::Top) {
            saveSettings();
            stack().popState();
        } else {
            mode_ = Mode::Top;
            menu_.setCursor(0);
            rebuild();
        }
    }
}

void SettingsState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Settings", w, p.crystal);

    // The submenus are short; one inset panel sized to the longest list (Display,
    // 6 rows) keeps a stable frame across modes.
    const int rows = static_cast<int>(menu_.size());
    const int panelRows = rows < 6 ? 6 : rows;
    ui::drawFrame(54, 26, w - 108, panelRows * 12 + 10, ui::FrameStyle::Inset);
    ui::drawMenu(menu_, 70, 31, 12, style::kFontMenu, p.text, p.disabled, p.cursor);

    if (!message_.empty()) {
        ui::drawBanner(ui::BannerKind::Success, message_, 60, h - 40, w - 120,
                       "settings.message");
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const char* backLabel = mode_ == Mode::Top ? "Save & Back" : "Back";
    ui::drawFooterHints(
        {{input::primaryLabel(map, InputAction::MoveLeft, device) + "/" +
              input::primaryLabel(map, InputAction::MoveRight, device),
          "Adjust"},
         {input::primaryLabel(map, InputAction::Confirm, device), "Select"},
         {input::primaryLabel(map, InputAction::Cancel, device), backLabel}},
        w, h, "settings.footer");
}

}  // namespace cd
