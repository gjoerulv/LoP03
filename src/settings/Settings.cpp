#include "settings/Settings.hpp"

#include "platform/AtomicFile.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "content/JsonValidation.hpp"
#include "input/Remap.hpp"

namespace cd::settings {

namespace fs = std::filesystem;
using content::Json;

std::string_view battleSpeedName(BattleSpeed s) {
  switch (s) {
  case BattleSpeed::Normal: return "normal";
  case BattleSpeed::Fast: return "fast";
  case BattleSpeed::Instant: return "instant";
  }
  return "normal";
}

std::optional<BattleSpeed> battleSpeedFromName(std::string_view name) {
  if (name == "normal") return BattleSpeed::Normal;
  if (name == "fast") return BattleSpeed::Fast;
  if (name == "instant") return BattleSpeed::Instant;
  return std::nullopt;
}

std::string_view messageSpeedName(MessageSpeed s) {
  switch (s) {
  case MessageSpeed::Slow: return "slow";
  case MessageSpeed::Normal: return "normal";
  case MessageSpeed::Fast: return "fast";
  }
  return "normal";
}

std::optional<MessageSpeed> messageSpeedFromName(std::string_view name) {
  if (name == "slow") return MessageSpeed::Slow;
  if (name == "normal") return MessageSpeed::Normal;
  if (name == "fast") return MessageSpeed::Fast;
  return std::nullopt;
}

std::string_view effectLevelName(EffectLevel s) {
  switch (s) {
  case EffectLevel::Full: return "full";
  case EffectLevel::Reduced: return "reduced";
  case EffectLevel::Off: return "off";
  }
  return "full";
}

std::optional<EffectLevel> effectLevelFromName(std::string_view name) {
  if (name == "full") return EffectLevel::Full;
  if (name == "reduced") return EffectLevel::Reduced;
  if (name == "off") return EffectLevel::Off;
  return std::nullopt;
}

float resolveSeconds(BattleSpeed s) {
  switch (s) {
  case BattleSpeed::Normal: return 0.9f;
  case BattleSpeed::Fast: return 0.45f;
  case BattleSpeed::Instant: return 0.05f;
  }
  return 0.9f;
}

float messageDurationScale(MessageSpeed s) {
  switch (s) {
  case MessageSpeed::Slow: return 1.5f;
  case MessageSpeed::Normal: return 1.0f;
  case MessageSpeed::Fast: return 0.6f;
  }
  return 1.0f;
}

namespace {
constexpr const char* kSource = "settings.json";

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

// Reads one device's binding object ({"confirm": [257], ...}) into per-action
// code lists. Unknown actions and invalid codes are reported and skipped.
void parseDeviceBindings(const Json& obj, const char* device, bool isKeyboard,
                         std::array<std::vector<int>, kInputActionCount>& out,
                         std::array<bool, kInputActionCount>& present,
                         content::LoadReport& report) {
  for (const auto& [name, value] : obj.items()) {
    const auto action = actionFromName(name);
    if (!action || !input::isRemappable(*action)) {
      report.add(kSource, std::string("bindings.") + device,
                 "unknown or non-remappable action '" + name + "' ignored");
      continue;
    }
    if (!value.is_array()) {
      report.add(kSource, std::string("bindings.") + device + "." + name,
                 "expected an array of codes");
      continue;
    }

    std::vector<int> codes;
    for (const Json& code : value) {
      if (!code.is_number_integer()) {
        report.add(kSource, std::string("bindings.") + device + "." + name,
                   "non-integer code ignored");
        continue;
      }
      const int c = code.get<int>();
      const bool valid = isKeyboard ? (c > 0 && c < 1024) : (c >= 1 && c <= 17);
      if (!valid) {
        report.add(kSource, std::string("bindings.") + device + "." + name,
                   "out-of-range code " + std::to_string(c) + " ignored");
        continue;
      }
      codes.push_back(c);
    }
    out[actionIndex(*action)] = std::move(codes);
    present[actionIndex(*action)] = true;
  }
}

} // namespace

bool parseSettingsText(const std::string& text, Settings& values, InputMap& map,
                       content::LoadReport& report) {
  values = Settings{};
  Json root = Json::parse(text, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    report.add(kSource, "<root>", "not a JSON object; using defaults");
    return false;
  }

  content::ObjectReader rootReader(root, "settings", kSource, report);
  const int version = rootReader.reqInt("version");
  if (version != kSettingsVersion) {
    report.add(kSource, "version",
               "unsupported settings version " + std::to_string(version) +
                   "; using defaults");
    return false;
  }

  if (const auto it = root.find("audio"); it != root.end() && it->is_object()) {
    content::ObjectReader audio(*it, "settings.audio", kSource, report);
    values.masterVolume = clamp01(audio.optFloat("master", 1.0f));
    values.musicVolume = clamp01(audio.optFloat("music", 1.0f));
    values.sfxVolume = clamp01(audio.optFloat("sfx", 1.0f));
    // M52: optional; absent keeps the 0.5 default so pre-M52 files load unchanged.
    values.ambienceVolume = clamp01(audio.optFloat("ambience", 0.5f));
  }

  if (const auto it = root.find("display"); it != root.end() && it->is_object()) {
    const Json& display = *it;
    if (const auto b = display.find("borderless"); b != display.end()) {
      if (b->is_boolean()) {
        values.borderlessFullscreen = b->get<bool>();
      } else {
        report.add(kSource, "display.borderless", "expected a boolean");
      }
    }
  }

  if (const auto it = root.find("gameplay"); it != root.end() && it->is_object()) {
    content::ObjectReader gameplay(*it, "settings.gameplay", kSource, report);
    const std::string battle =
        gameplay.optString("battleSpeed", std::string(battleSpeedName(values.battleSpeed)));
    if (auto s = battleSpeedFromName(battle)) {
      values.battleSpeed = *s;
    } else {
      report.add(kSource, "gameplay.battleSpeed", "unknown value '" + battle + "'");
    }

    const std::string message = gameplay.optString(
        "messageSpeed", std::string(messageSpeedName(values.messageSpeed)));
    if (auto s = messageSpeedFromName(message)) {
      values.messageSpeed = *s;
    } else {
      report.add(kSource, "gameplay.messageSpeed", "unknown value '" + message + "'");
    }

    // M18 effect fields are optional; absent keeps the Full default so
    // pre-M18 settings files load unchanged.
    const std::string flash =
        gameplay.optString("effectFlash", std::string(effectLevelName(values.effectFlash)));
    if (auto s = effectLevelFromName(flash)) {
      values.effectFlash = *s;
    } else {
      report.add(kSource, "gameplay.effectFlash", "unknown value '" + flash + "'");
    }

    const std::string shake =
        gameplay.optString("effectShake", std::string(effectLevelName(values.effectShake)));
    if (auto s = effectLevelFromName(shake)) {
      values.effectShake = *s;
    } else {
      report.add(kSource, "gameplay.effectShake", "unknown value '" + shake + "'");
    }

    // M22: optional; absent keeps the standard palette.
    if (const auto hc = it->find("highContrast"); hc != it->end()) {
      if (hc->is_boolean()) {
        values.highContrast = hc->get<bool>();
      } else {
        report.add(kSource, "gameplay.highContrast", "expected a boolean");
      }
    }
    // M51: optional bools; absent = false so older files load unchanged.
    const auto optBool = [&](const char* key, bool& out) {
      if (const auto f = it->find(key); f != it->end()) {
        if (f->is_boolean()) {
          out = f->get<bool>();
        } else {
          report.add(kSource, std::string("gameplay.") + key, "expected a boolean");
        }
      }
    };
    optBool("crtEffect", values.crtEffect);
    optBool("backgroundAudio", values.backgroundAudio);
  }

  if (const auto it = root.find("bindings"); it != root.end() && it->is_object()) {
    std::array<std::vector<int>, kInputActionCount> keyLists{};
    std::array<std::vector<int>, kInputActionCount> buttonLists{};
    std::array<bool, kInputActionCount> keyPresent{};
    std::array<bool, kInputActionCount> buttonPresent{};
    if (const auto kb = it->find("keyboard"); kb != it->end() && kb->is_object()) {
      parseDeviceBindings(*kb, "keyboard", true, keyLists, keyPresent, report);
    }
    if (const auto gp = it->find("gamepad"); gp != it->end() && gp->is_object()) {
      parseDeviceBindings(*gp, "gamepad", false, buttonLists, buttonPresent, report);
    }

    const InputMap defaults;
    for (InputAction a : kRemappableActions) {
      const std::size_t i = actionIndex(a);
      if (!keyPresent[i] && !buttonPresent[i]) {
        continue; // action not mentioned: keep defaults
      }
      std::vector<int> keys = keyPresent[i] ? keyLists[i] : defaults.keys(a);
      std::vector<int> buttons = buttonPresent[i] ? buttonLists[i] : defaults.buttons(a);

      // Never strand the player: a remappable action must keep at least
      // one keyboard key.
      if (keys.empty()) {
        report.add(kSource, "bindings.keyboard",
                   "action '" + std::string(actionName(a)) +
                       "' had no keyboard binding; defaults restored");
        keys = defaults.keys(a);
      }
      map.clear(a);
      for (int key : keys) {
        map.bindKey(a, key);
      }
      for (int btn : buttons) {
        map.bindButton(a, btn);
      }
    }
  }

  return true;
}

std::string serializeSettings(const Settings& values, const InputMap& map) {
  Json root;
  root["version"] = kSettingsVersion;
  root["audio"] = {{"master", values.masterVolume},
                   {"music", values.musicVolume},
                   {"sfx", values.sfxVolume},
                   {"ambience", values.ambienceVolume}};
  root["display"] = {{"borderless", values.borderlessFullscreen}};
  root["gameplay"] = {{"battleSpeed", std::string(battleSpeedName(values.battleSpeed))},
                      {"messageSpeed", std::string(messageSpeedName(values.messageSpeed))},
                      {"effectFlash", std::string(effectLevelName(values.effectFlash))},
                      {"effectShake", std::string(effectLevelName(values.effectShake))},
                      {"highContrast", values.highContrast},
                      {"crtEffect", values.crtEffect},
                      {"backgroundAudio", values.backgroundAudio}};

  Json keyboard = Json::object();
  Json gamepad = Json::object();
  for (InputAction a : kRemappableActions) {
    keyboard[std::string(actionName(a))] = map.keys(a);
    gamepad[std::string(actionName(a))] = map.buttons(a);
  }
  root["bindings"] = {{"keyboard", std::move(keyboard)}, {"gamepad", std::move(gamepad)}};
  return root.dump(2) + "\n";
}

SettingsStore::SettingsStore(fs::path file) : file_(std::move(file)) {}

bool SettingsStore::load(InputMap& map, content::LoadReport& report) {
  std::error_code ec;
  if (!fs::exists(file_, ec) || ec) {
    values = Settings{};
    return true; // first run: silent defaults
  }
  std::ifstream in(file_, std::ios::binary);
  if (!in) {
    report.add(kSource, "<file>", "could not open settings file; using defaults");
    values = Settings{};
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return parseSettingsText(buffer.str(), values, map, report);
}

bool SettingsStore::save(const InputMap& map, content::LoadReport& report) const {
  std::string writeError;
  if (!platform::writeTextFileAtomically(file_, serializeSettings(values, map), writeError)) {
    report.add(kSource, "", "could not save settings atomically: " + writeError);
    return false;
  }
  return true;
}

} // namespace cd::settings
