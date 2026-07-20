#include <catch2/catch_test_macros.hpp>

#include <string>

#include "content/LoadReport.hpp"
#include "input/InputAction.hpp"
#include "input/InputMap.hpp"
#include "settings/Settings.hpp"
#include "ui/UiStyle.hpp"

namespace style = cd::ui::style;

namespace {
bool sameColor(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
}  // namespace

TEST_CASE("standard palette matches the legacy constants exactly", "[accessibility]") {
    style::setHighContrast(false);
    const style::Palette& p = style::palette();
    CHECK(sameColor(p.text, style::kText));
    CHECK(sameColor(p.textDim, style::kTextDim));
    CHECK(sameColor(p.textHint, style::kTextHint));
    CHECK(sameColor(p.disabled, style::kDisabled));
    CHECK(sameColor(p.cursor, style::kCursor));
    CHECK(sameColor(p.success, style::kSuccess));
    CHECK(sameColor(p.dangerText, style::kDangerText));
    CHECK(sameColor(p.gold, style::kGold));
}

TEST_CASE("high-contrast palette switches and is strictly brighter text", "[accessibility]") {
    style::setHighContrast(true);
    CHECK(style::highContrastActive());
    const style::Palette& p = style::palette();
    CHECK(p.text.r == 255);
    CHECK(p.text.g == 255);
    CHECK(p.text.b == 255);
    // Every text-carrying role must be at least as bright as its standard
    // counterpart (the whole point of the mode).
    CHECK(p.textDim.r >= style::kTextDim.r);
    CHECK(p.textHint.r >= style::kTextHint.r);
    CHECK(p.disabled.r >= style::kDisabled.r);
    style::setHighContrast(false);  // restore for other tests
    CHECK_FALSE(style::highContrastActive());
}

TEST_CASE("settings: highContrast round-trips and defaults to off", "[accessibility]") {
    using cd::content::LoadReport;
    cd::settings::Settings values;
    cd::InputMap map;  // default ctor installs default bindings

    // Absent field (pre-M22 file) keeps the standard palette.
    {
        cd::settings::Settings parsed;
        parsed.highContrast = true;  // must be overwritten by the default
        cd::InputMap m;
        LoadReport report;
        // parse resets to defaults first, so an absent field means false.
        REQUIRE(cd::settings::parseSettingsText(R"({"version": 1})", parsed, m, report));
        CHECK_FALSE(parsed.highContrast);
    }

    values.highContrast = true;
    const std::string text = cd::settings::serializeSettings(values, map);
    cd::settings::Settings back;
    cd::InputMap map2;
    LoadReport report;
    REQUIRE(cd::settings::parseSettingsText(text, back, map2, report));
    CHECK(back.highContrast);

    // Wrong type reports and keeps the default.
    {
        cd::settings::Settings parsed;
        cd::InputMap m;
        LoadReport r2;
        cd::settings::parseSettingsText(
            R"({"version": 1, "gameplay": {"highContrast": "yes"}})", parsed, m, r2);
        CHECK_FALSE(parsed.highContrast);
        CHECK(r2.errorCount() > 0);
    }
}

TEST_CASE("Details is a first-class remappable action with defaults", "[accessibility]") {
    // Stable name (public schema) and display name.
    CHECK(cd::actionName(cd::InputAction::Details) == "details");
    CHECK(cd::actionFromName("details") == cd::InputAction::Details);
    CHECK(cd::actionDisplayName(cd::InputAction::Details) == "Details / Info");

    bool remappable = false;
    for (cd::InputAction a : cd::kRemappableActions) {
        if (a == cd::InputAction::Details) {
            remappable = true;
        }
    }
    CHECK(remappable);

    // Default bindings exist on both devices (prompt labels depend on this).
    const cd::InputMap map;  // default ctor installs default bindings
    CHECK_FALSE(map.keys(cd::InputAction::Details).empty());
    CHECK_FALSE(map.buttons(cd::InputAction::Details).empty());
}
