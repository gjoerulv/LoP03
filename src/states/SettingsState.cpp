#include "states/SettingsState.hpp"

#include <memory>
#include <string>

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
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kMasterRow = 0;
constexpr int kMusicRow = 1;
constexpr int kSfxRow = 2;
constexpr int kWindowRow = 3;
constexpr int kBattleSpeedRow = 4;
constexpr int kMessageSpeedRow = 5;
constexpr int kEffectFlashRow = 6;
constexpr int kEffectShakeRow = 7;
constexpr int kRemapKeyboardRow = 8;
constexpr int kRemapGamepadRow = 9;
constexpr int kResetRow = 10;
constexpr int kBackRow = 11;

int volumeSteps(float v) { return static_cast<int>(v * 10.0f + 0.5f); }

std::string volumeLabel(const char* name, float v) {
    return std::string(name) + ":  < " + std::to_string(volumeSteps(v)) + " >";
}
}  // namespace

SettingsState::SettingsState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void SettingsState::onEnter() { rebuild(); }
void SettingsState::onResume() { rebuild(); }

void SettingsState::rebuild() {
    const settings::Settings& v = context_.settings.values;
    std::vector<ui::MenuItem> items;
    items.push_back({volumeLabel("Master Volume", v.masterVolume), true});
    items.push_back({volumeLabel("Music Volume", v.musicVolume), true});
    items.push_back({volumeLabel("SFX Volume", v.sfxVolume), true});
    items.push_back({std::string("Window:  < ") +
                         (v.borderlessFullscreen ? "Borderless" : "Windowed") + " >",
                     true});
    items.push_back({std::string("Battle Speed:  < ") +
                         std::string(settings::battleSpeedName(v.battleSpeed)) + " >",
                     true});
    items.push_back({std::string("Message Speed:  < ") +
                         std::string(settings::messageSpeedName(v.messageSpeed)) + " >",
                     true});
    items.push_back({std::string("Battle Flash:  < ") +
                         std::string(settings::effectLevelName(v.effectFlash)) + " >",
                     true});
    items.push_back({std::string("Battle Shake:  < ") +
                         std::string(settings::effectLevelName(v.effectShake)) + " >",
                     true});
    items.push_back({"Remap Keyboard...", true});
    items.push_back({"Remap Gamepad...", context_.input.gamepadAvailable()});
    items.push_back({"Reset settings and bindings", true});
    items.push_back({"Back", true});
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

void SettingsState::adjust(int row, int direction) {
    settings::Settings& v = context_.settings.values;
    const auto step = [direction](float value) {
        float next = value + 0.1f * static_cast<float>(direction);
        return next < 0.0f ? 0.0f : (next > 1.0f ? 1.0f : next);
    };
    switch (row) {
        case kMasterRow: v.masterVolume = step(v.masterVolume); applyAudio(); break;
        case kMusicRow: v.musicVolume = step(v.musicVolume); applyAudio(); break;
        case kSfxRow: v.sfxVolume = step(v.sfxVolume); applyAudio(); break;
        case kWindowRow: v.borderlessFullscreen = !v.borderlessFullscreen; break;
        case kBattleSpeedRow: {
            int s = static_cast<int>(v.battleSpeed) + direction;
            s = (s % 3 + 3) % 3;
            v.battleSpeed = static_cast<settings::BattleSpeed>(s);
            break;
        }
        case kMessageSpeedRow: {
            int s = static_cast<int>(v.messageSpeed) + direction;
            s = (s % 3 + 3) % 3;
            v.messageSpeed = static_cast<settings::MessageSpeed>(s);
            break;
        }
        case kEffectFlashRow: {
            int s = static_cast<int>(v.effectFlash) + direction;
            s = (s % 3 + 3) % 3;
            v.effectFlash = static_cast<settings::EffectLevel>(s);
            break;
        }
        case kEffectShakeRow: {
            int s = static_cast<int>(v.effectShake) + direction;
            s = (s % 3 + 3) % 3;
            v.effectShake = static_cast<settings::EffectLevel>(s);
            break;
        }
        default: return;
    }
    context_.audio.play(Sfx::Move);
    rebuild();
}

void SettingsState::activate(int row) {
    switch (row) {
        case kRemapKeyboardRow:
            stack().pushState(std::make_unique<RemapState>(stack(), context_,
                                                           ActiveDevice::Keyboard));
            break;
        case kRemapGamepadRow:
            stack().pushState(
                std::make_unique<RemapState>(stack(), context_, ActiveDevice::Gamepad));
            break;
        case kResetRow:
            context_.settings.values = settings::Settings{};
            input::resetBindings(context_.input.map());
            applyAudio();
            saveSettings();
            message_ = "Settings and bindings reset to defaults";
            rebuild();
            break;
        case kBackRow:
            saveSettings();
            stack().popState();
            break;
        default:
            break;
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
        adjust(menu_.cursor(), dir);
    }
    if (input.pressed(InputAction::Confirm) && menu_.currentEnabled()) {
        context_.audio.play(Sfx::Confirm);
        activate(menu_.cursor());
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        saveSettings();
        stack().popState();
    }
}

void SettingsState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{16, 16, 26, 255});
    ui::drawTextCentered("Settings", w / 2, 14, style::kFontScreenTitle, style::kText);

    // 12 rows at 14px spacing keep the taller M18 menu inside 240px.
    ui::drawMenu(menu_, 70, 36, 14, style::kFontMenu, style::kText, style::kDisabled,
                 style::kCursor);

    if (!message_.empty()) {
        ui::drawTextFitted(message_, 40, h - 34, w - 80, style::kFontBody, style::kSuccess,
                           "settings.message");
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    const std::string footer =
        input::prompt(map, InputAction::MoveLeft, device, "/") + " " +
        input::prompt(map, InputAction::MoveRight, device, "Adjust") + "    " +
        input::prompt(map, InputAction::Confirm, device, "Select") + "    " +
        input::prompt(map, InputAction::Cancel, device, "Save & Back");
    ui::drawTextCentered(footer.c_str(), w / 2, h - style::kFooterHeight + 2, 9,
                         style::kTextHint);
}

}  // namespace cd
