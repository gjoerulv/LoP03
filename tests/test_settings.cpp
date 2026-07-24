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

TEST_CASE("settings: M57 crtIntensity round-trips and defaults to 0 when absent") {
    Settings values;
    values.crtIntensity = 0.6f;
    values.backgroundAudio = true;
    InputMap map;
    const std::string text = serializeSettings(values, map);

    Settings loaded;
    InputMap loadedMap;
    LoadReport report;
    REQUIRE(parseSettingsText(text, loaded, loadedMap, report));
    CHECK(report.errorCount() == 0);
    CHECK(loaded.crtIntensity == 0.6f);
    CHECK(loaded.backgroundAudio);

    // A file lacking crtIntensity (and any legacy crtEffect) loads at 0.0.
    Settings absent;
    LoadReport reportAbsent;
    REQUIRE(parseSettingsText(
        R"({"version":1,"gameplay":{"battleSpeed":"fast","messageSpeed":"normal"}})", absent,
        loadedMap, reportAbsent));
    CHECK(reportAbsent.errorCount() == 0);
    CHECK(absent.crtIntensity == 0.0f);
    CHECK_FALSE(absent.backgroundAudio);
}

TEST_CASE("settings: M57 crtIntensity clamps out-of-range values") {
    InputMap map;
    Settings low;
    LoadReport rlow;
    REQUIRE(parseSettingsText(R"({"version":1,"gameplay":{"crtIntensity":-0.5}})", low, map, rlow));
    CHECK(low.crtIntensity == 0.0f);

    Settings high;
    LoadReport rhigh;
    REQUIRE(parseSettingsText(R"({"version":1,"gameplay":{"crtIntensity":1.7}})", high, map, rhigh));
    CHECK(high.crtIntensity == 1.0f);
}

TEST_CASE("settings: M57 legacy crtEffect bool migrates defensively") {
    InputMap map;

    // Legacy false -> 0.0.
    Settings off;
    LoadReport roff;
    REQUIRE(parseSettingsText(R"({"version":1,"gameplay":{"crtEffect":false}})", off, map, roff));
    CHECK(roff.errorCount() == 0);
    CHECK(off.crtIntensity == 0.0f);

    // Legacy true -> 0.3 (approximately preserves the old subtle effect).
    Settings on;
    LoadReport ron;
    REQUIRE(parseSettingsText(R"({"version":1,"gameplay":{"crtEffect":true}})", on, map, ron));
    CHECK(ron.errorCount() == 0);
    CHECK(on.crtIntensity == 0.3f);

    // A new numeric crtIntensity takes precedence over a legacy bool when both
    // are present.
    Settings both;
    LoadReport rboth;
    REQUIRE(parseSettingsText(
        R"({"version":1,"gameplay":{"crtIntensity":0.7,"crtEffect":true}})", both, map, rboth));
    CHECK(rboth.errorCount() == 0);
    CHECK(both.crtIntensity == 0.7f);
}

TEST_CASE("settings: M57 malformed crtIntensity is reported and falls back safely") {
    InputMap map;
    Settings bad;
    LoadReport report;
    REQUIRE(parseSettingsText(R"({"version":1,"gameplay":{"crtIntensity":"loud"}})", bad, map,
                              report));
    CHECK(report.errorCount() > 0);
    CHECK(bad.crtIntensity == 0.0f);  // safe default
}

TEST_CASE("settings: M57 crt strength <-> intensity step conversion") {
    CHECK(crtStrengthStep(0.0f) == 0);
    CHECK(crtStrengthStep(0.3f) == 3);
    CHECK(crtStrengthStep(1.0f) == 10);
    CHECK(crtStrengthStep(1.5f) == 10);   // clamped
    CHECK(crtStrengthStep(-1.0f) == 0);   // clamped

    CHECK(crtIntensityFromStep(0) == 0.0f);
    CHECK(crtIntensityFromStep(3) == 0.3f);
    CHECK(crtIntensityFromStep(10) == 1.0f);
    CHECK(crtIntensityFromStep(15) == 1.0f);  // clamped
    CHECK(crtIntensityFromStep(-2) == 0.0f);  // clamped

    // A default (reset) Settings has strength 0.
    CHECK(crtStrengthStep(Settings{}.crtIntensity) == 0);
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
