#include <catch2/catch_test_macros.hpp>

#include "content/LoadReport.hpp"
#include "input/InputMap.hpp"
#include "settings/Settings.hpp"

using cd::InputAction;
using cd::InputMap;
using cd::content::LoadReport;
using namespace cd::settings;

TEST_CASE("settings: serialize/parse round-trip preserves values and bindings") {
    Settings values;
    values.masterVolume = 0.7f;
    values.musicVolume = 0.4f;
    values.sfxVolume = 0.9f;
    values.borderlessFullscreen = true;
    values.battleSpeed = BattleSpeed::Fast;
    values.messageSpeed = MessageSpeed::Slow;
    InputMap map;
    map.clear(InputAction::Confirm);
    map.bindKey(InputAction::Confirm, 74);  // 'J'
    map.bindButton(InputAction::Confirm, 5);

    const std::string text = serializeSettings(values, map);

    Settings loaded;
    InputMap loadedMap;
    LoadReport report;
    REQUIRE(parseSettingsText(text, loaded, loadedMap, report));
    CHECK(report.errorCount() == 0);
    CHECK(loaded.masterVolume == 0.7f);
    CHECK(loaded.musicVolume == 0.4f);
    CHECK(loaded.sfxVolume == 0.9f);
    CHECK(loaded.borderlessFullscreen);
    CHECK(loaded.battleSpeed == BattleSpeed::Fast);
    CHECK(loaded.messageSpeed == MessageSpeed::Slow);
    REQUIRE(loadedMap.keys(InputAction::Confirm).size() == 1);
    CHECK(loadedMap.keys(InputAction::Confirm).front() == 74);
    REQUIRE(loadedMap.buttons(InputAction::Confirm).size() == 1);
    CHECK(loadedMap.buttons(InputAction::Confirm).front() == 5);
}

TEST_CASE("settings: effect levels round-trip and default to full when absent") {
    Settings values;
    values.effectFlash = EffectLevel::Off;
    values.effectShake = EffectLevel::Reduced;
    InputMap map;
    const std::string text = serializeSettings(values, map);

    Settings loaded;
    InputMap loadedMap;
    LoadReport report;
    REQUIRE(parseSettingsText(text, loaded, loadedMap, report));
    CHECK(report.errorCount() == 0);
    CHECK(loaded.effectFlash == EffectLevel::Off);
    CHECK(loaded.effectShake == EffectLevel::Reduced);

    // Pre-M18 file: gameplay object without effect fields keeps Full.
    Settings old;
    LoadReport report2;
    REQUIRE(parseSettingsText(
        R"({"version":1,"gameplay":{"battleSpeed":"fast","messageSpeed":"normal"}})", old,
        loadedMap, report2));
    CHECK(report2.errorCount() == 0);
    CHECK(old.effectFlash == EffectLevel::Full);
    CHECK(old.effectShake == EffectLevel::Full);

    // Unknown value: reported, default kept.
    Settings bad;
    LoadReport report3;
    REQUIRE(parseSettingsText(
        R"({"version":1,"gameplay":{"effectFlash":"strobe"}})", bad, loadedMap, report3));
    CHECK(report3.errorCount() == 1);
    CHECK(bad.effectFlash == EffectLevel::Full);
}

TEST_CASE("settings: M51 crtEffect/backgroundAudio round-trip and default off when absent") {
    Settings values;
    values.crtEffect = true;
    values.backgroundAudio = true;
    InputMap map;
    const std::string text = serializeSettings(values, map);

    Settings loaded;
    InputMap loadedMap;
    LoadReport report;
    REQUIRE(parseSettingsText(text, loaded, loadedMap, report));
    CHECK(report.errorCount() == 0);
    CHECK(loaded.crtEffect);
    CHECK(loaded.backgroundAudio);

    // A pre-M51 file lacking both fields loads with both OFF (the defaults), so
    // older settings.json is unaffected.
    Settings old;
    LoadReport report2;
    REQUIRE(parseSettingsText(
        R"({"version":1,"gameplay":{"battleSpeed":"fast","messageSpeed":"normal"}})", old,
        loadedMap, report2));
    CHECK(report2.errorCount() == 0);
    CHECK_FALSE(old.crtEffect);
    CHECK_FALSE(old.backgroundAudio);

    // A non-boolean value is reported but survivable (keeps the default).
    Settings bad;
    LoadReport report3;
    REQUIRE(parseSettingsText(
        R"({"version":1,"gameplay":{"crtEffect":"yes"}})", bad, loadedMap, report3));
    CHECK(report3.errorCount() > 0);
    CHECK_FALSE(bad.crtEffect);
}

TEST_CASE("settings: malformed JSON yields defaults and a report") {
    Settings values;
    InputMap map;
    LoadReport report;
    CHECK_FALSE(parseSettingsText("{not json", values, map, report));
    CHECK(report.errorCount() > 0);
    CHECK(values.masterVolume == 1.0f);
    CHECK(values.battleSpeed == BattleSpeed::Normal);
}

TEST_CASE("settings: foreign version yields defaults and a report") {
    Settings values;
    InputMap map;
    LoadReport report;
    CHECK_FALSE(parseSettingsText(R"({"version": 99})", values, map, report));
    CHECK(report.errorCount() > 0);
}

TEST_CASE("settings: unknown fields are ignored, bad values reported but survivable") {
    Settings values;
    InputMap map;
    LoadReport report;
    const std::string text = R"({
        "version": 1,
        "someFutureBlock": {"x": 1},
        "audio": {"master": 2.5, "music": -1.0},
        "gameplay": {"battleSpeed": "warp"}
    })";
    CHECK(parseSettingsText(text, values, map, report));
    CHECK(values.masterVolume == 1.0f);   // clamped
    CHECK(values.musicVolume == 0.0f);    // clamped
    CHECK(values.battleSpeed == BattleSpeed::Normal);  // unknown value reported
    CHECK(report.errorCount() > 0);
}

TEST_CASE("settings: a binding set stranding the keyboard is restored to defaults") {
    Settings values;
    InputMap map;
    LoadReport report;
    const std::string text = R"({
        "version": 1,
        "bindings": {"keyboard": {"confirm": []}}
    })";
    CHECK(parseSettingsText(text, values, map, report));
    CHECK(report.errorCount() > 0);
    CHECK_FALSE(map.keys(InputAction::Confirm).empty());  // defaults restored
}

TEST_CASE("settings: out-of-range and unknown binding entries are skipped") {
    Settings values;
    InputMap map;
    LoadReport report;
    const std::string text = R"({
        "version": 1,
        "bindings": {
            "keyboard": {"confirm": [74, -3, 90000], "warp_drive": [1]},
            "gamepad": {"confirm": [7, 99]}
        }
    })";
    CHECK(parseSettingsText(text, values, map, report));
    REQUIRE(map.keys(InputAction::Confirm).size() == 1);
    CHECK(map.keys(InputAction::Confirm).front() == 74);
    REQUIRE(map.buttons(InputAction::Confirm).size() == 1);
    CHECK(map.buttons(InputAction::Confirm).front() == 7);
    CHECK(report.errorCount() > 0);  // bad codes + unknown action reported
}

TEST_CASE("settings: store round-trips through a real file") {
    namespace fs = std::filesystem;
    const fs::path file = fs::temp_directory_path() / "cd_test_settings" / "settings.json";
    std::error_code ec;
    fs::remove_all(file.parent_path(), ec);

    SettingsStore store(file);
    store.values.masterVolume = 0.25f;
    store.values.battleSpeed = BattleSpeed::Instant;
    InputMap map;
    LoadReport saveReport;
    REQUIRE(store.save(map, saveReport));

    SettingsStore loaded(file);
    InputMap loadedMap;
    LoadReport loadReport;
    REQUIRE(loaded.load(loadedMap, loadReport));
    CHECK(loaded.values.masterVolume == 0.25f);
    CHECK(loaded.values.battleSpeed == BattleSpeed::Instant);

    fs::remove_all(file.parent_path(), ec);
}

TEST_CASE("settings: missing file is silent defaults") {
    namespace fs = std::filesystem;
    SettingsStore store(fs::temp_directory_path() / "cd_test_settings_missing" /
                        "settings.json");
    InputMap map;
    LoadReport report;
    CHECK(store.load(map, report));
    CHECK(report.errorCount() == 0);
    CHECK(store.values.masterVolume == 1.0f);
}

TEST_CASE("settings: speed helpers") {
    CHECK(resolveSeconds(BattleSpeed::Normal) > resolveSeconds(BattleSpeed::Fast));
    CHECK(resolveSeconds(BattleSpeed::Fast) > resolveSeconds(BattleSpeed::Instant));
    CHECK(messageDurationScale(MessageSpeed::Slow) > messageDurationScale(MessageSpeed::Fast));
    CHECK(battleSpeedFromName("fast") == BattleSpeed::Fast);
    CHECK_FALSE(battleSpeedFromName("bogus").has_value());
}
