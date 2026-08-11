// M79 — input & QoL: the party-cycling actions and their defaults, the
// three-slot keyboard remap (direct slots, steal confirmation, the
// never-unbound invariant), the fresh-profile volume defaults, and the
// tightened Decisive achievement.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "game/Achievements.hpp"
#include "game/Party.hpp"
#include "input/PromptLabels.hpp"
#include "input/Remap.hpp"
#include "settings/Settings.hpp"

using cd::InputAction;
using cd::InputMap;
using namespace cd::input;

namespace {
// raylib codes as plain ints (this file stays raylib-free like test_remap).
constexpr int kKeyEnter = 257;
constexpr int kKeySpace = 32;
constexpr int kKeyTab = 258;
constexpr int kKeyUp = 265;
constexpr int kKeyQ = 81;
constexpr int kKeyE = 69;
constexpr int kKeyW = 87;
constexpr int kKeyZ = 90;
constexpr int kKeyX = 88;
constexpr int kKeyJ = 74;
constexpr int kKeyV = 86;
constexpr int kKeyC = 67;
constexpr int kKeyEscape = 256;
constexpr int kKeyLeftControl = 341;
constexpr int kKeyLeftAlt = 342;
constexpr int kButtonLB = 9;
constexpr int kButtonRB = 11;
}  // namespace

// --- the M79 defaults ---------------------------------------------------------

TEST_CASE("qol: the cycling pair and the Z/X alternates are the fresh defaults",
          "[qol]") {
    const InputMap map;
    CHECK(map.keys(InputAction::CyclePrev) ==
          std::vector<int>{kKeyQ, kKeyLeftControl});
    CHECK(map.keys(InputAction::CycleNext) == std::vector<int>{kKeyE, kKeyLeftAlt});
    CHECK(map.buttons(InputAction::CyclePrev) == std::vector<int>{kButtonLB});
    CHECK(map.buttons(InputAction::CycleNext) == std::vector<int>{kButtonRB});
    // Confirm/Cancel gain their third slot (owner decision: Z and X).
    CHECK(map.keys(InputAction::Confirm) ==
          std::vector<int>{kKeyEnter, kKeySpace, kKeyZ});
    CHECK(map.keys(InputAction::Cancel).back() == kKeyX);
    // Both cycling actions are remappable and carry serialization names.
    CHECK(isRemappable(InputAction::CyclePrev));
    CHECK(isRemappable(InputAction::CycleNext));
    CHECK(cd::actionFromName("cycle_prev") == InputAction::CyclePrev);
    CHECK(cd::actionFromName("cycle_next") == InputAction::CycleNext);
    // The modifier alternates have real prompt names, not "Key#341".
    CHECK(keyName(kKeyLeftControl) == "Ctrl");
    CHECK(keyName(kKeyLeftAlt) == "Alt");
}

TEST_CASE("qol: the numpad and navigation cluster have real key names", "[qol]") {
    // Owner feedback (2026-08-06): these used to render as "Key#328"-style
    // fallbacks in the remap columns and every prompt.
    CHECK(keyName(320) == "Num 0");
    CHECK(keyName(328) == "Num 8");
    CHECK(keyName(335) == "Num Enter");
    CHECK(keyName(334) == "Num +");
    CHECK(keyName(331) == "Num /");
    CHECK(keyName(283) == "PrtScr");
    CHECK(keyName(268) == "Home");
    CHECK(keyName(266) == "PgUp");
    CHECK(keyName(261) == "Delete");
    CHECK(keyName(282) == "NumLock");
}

// --- the three direct slots ---------------------------------------------------

TEST_CASE("qol: a slot remap replaces exactly that slot", "[qol]") {
    InputMap map;
    const SlotResult r = assignKeySlot(map, InputAction::Confirm, 0, kKeyJ, false);
    CHECK(r.outcome == SlotOutcome::Rebound);
    CHECK(map.keys(InputAction::Confirm) == std::vector<int>{kKeyJ, kKeySpace, kKeyZ});
    // Gamepad bindings untouched.
    CHECK_FALSE(map.buttons(InputAction::Confirm).empty());
}

TEST_CASE("qol: an empty slot appends, and slots pack left", "[qol]") {
    InputMap map;  // Details has exactly one key (C)
    REQUIRE(map.keys(InputAction::Details) == std::vector<int>{kKeyC});
    // Asking for Alt 2 with Alt 1 empty lands in the next free slot.
    const SlotResult r = assignKeySlot(map, InputAction::Details, 2, kKeyV, false);
    CHECK(r.outcome == SlotOutcome::Rebound);
    CHECK(map.keys(InputAction::Details) == std::vector<int>{kKeyC, kKeyV});
}

TEST_CASE("qol: moving a key between an action's own slots needs no confirmation",
          "[qol]") {
    InputMap map;
    const SlotResult r = assignKeySlot(map, InputAction::Confirm, 2, kKeyEnter, false);
    CHECK(r.outcome == SlotOutcome::Rebound);
    CHECK(map.keys(InputAction::Confirm) == std::vector<int>{kKeySpace, kKeyZ, kKeyEnter});
}

TEST_CASE("qol: an in-use key warns first, then steals on confirmation", "[qol]") {
    InputMap map;
    // W belongs to Move Up (slot 1). First call: warning, nothing changes.
    const std::vector<int> before = map.keys(InputAction::MoveUp);
    const SlotResult warn = assignKeySlot(map, InputAction::Details, 1, kKeyW, false);
    CHECK(warn.outcome == SlotOutcome::NeedsConfirm);
    CHECK(warn.owner == InputAction::MoveUp);
    CHECK(warn.ownerSlot == 1);
    CHECK(map.keys(InputAction::MoveUp) == before);
    CHECK(map.keys(InputAction::Details) == std::vector<int>{kKeyC});

    // Confirmed: the key moves; Move Up keeps its other key (never unbound).
    const SlotResult steal = assignKeySlot(map, InputAction::Details, 1, kKeyW, true);
    CHECK(steal.outcome == SlotOutcome::Stolen);
    CHECK(steal.owner == InputAction::MoveUp);
    CHECK(map.keys(InputAction::Details) == std::vector<int>{kKeyC, kKeyW});
    CHECK(map.keys(InputAction::MoveUp) == std::vector<int>{kKeyUp});
}

TEST_CASE("qol: stealing an action's only key is blocked outright", "[qol]") {
    InputMap map;  // Menu's only key is Tab
    const SlotResult r = assignKeySlot(map, InputAction::Confirm, 0, kKeyTab, true);
    CHECK(r.outcome == SlotOutcome::Blocked);
    CHECK(r.owner == InputAction::Menu);
    CHECK(map.keys(InputAction::Menu) == std::vector<int>{kKeyTab});  // untouched
    CHECK(map.keys(InputAction::Confirm) ==
          std::vector<int>{kKeyEnter, kKeySpace, kKeyZ});
}

TEST_CASE("qol: Esc, non-remappables and out-of-range slots stay blocked", "[qol]") {
    InputMap map;
    CHECK(assignKeySlot(map, InputAction::Confirm, 0, kKeyEscape, false).outcome ==
          SlotOutcome::Blocked);
    CHECK(assignKeySlot(map, InputAction::ToggleDebug, 0, kKeyJ, false).outcome ==
          SlotOutcome::Blocked);
    CHECK(assignKeySlot(map, InputAction::Confirm, kKeySlotCount, kKeyJ, false).outcome ==
          SlotOutcome::Blocked);
}

// --- persistence --------------------------------------------------------------

TEST_CASE("qol: three keyboard slots round-trip settings; old files gain the defaults",
          "[qol]") {
    using namespace cd::settings;
    InputMap map;
    assignKeySlot(map, InputAction::Details, 2, kKeyV, false);
    Settings values;
    const std::string text = serializeSettings(values, map);

    Settings loaded;
    InputMap lm;
    cd::content::LoadReport rep;
    REQUIRE(parseSettingsText(text, loaded, lm, rep));
    CHECK(rep.errorCount() == 0);
    CHECK(lm.keys(InputAction::Confirm) ==
          std::vector<int>{kKeyEnter, kKeySpace, kKeyZ});
    CHECK(lm.keys(InputAction::Details) == std::vector<int>{kKeyC, kKeyV});
    CHECK(lm.keys(InputAction::CyclePrev) ==
          std::vector<int>{kKeyQ, kKeyLeftControl});

    // A pre-M79 file never mentions the cycling actions: the defaults stand.
    Settings old;
    InputMap om;
    cd::content::LoadReport rep2;
    REQUIRE(parseSettingsText(
        R"({"version":1,"bindings":{"keyboard":{"confirm":[257]},"gamepad":{}}})", old, om,
        rep2));
    CHECK(om.keys(InputAction::Confirm) == std::vector<int>{kKeyEnter});
    CHECK(om.keys(InputAction::CyclePrev) ==
          std::vector<int>{kKeyQ, kKeyLeftControl});
    CHECK(om.buttons(InputAction::CycleNext) == std::vector<int>{kButtonRB});
}

// --- the fresh-profile defaults ------------------------------------------------

TEST_CASE("qol: a fresh profile hears music at 7/10 and ambience at 3/10", "[qol]") {
    const cd::settings::Settings fresh;
    CHECK(fresh.musicVolume == 0.7f);
    CHECK(fresh.ambienceVolume == 0.3f);
    CHECK(fresh.masterVolume == 1.0f);  // untouched
    CHECK(fresh.sfxVolume == 1.0f);     // untouched

    // A written value is the player's and always wins over the new default.
    cd::settings::Settings loaded;
    InputMap lm;
    cd::content::LoadReport rep;
    REQUIRE(cd::settings::parseSettingsText(
        R"({"version":1,"audio":{"music":1.0,"ambience":0.5}})", loaded, lm, rep));
    CHECK(loaded.musicVolume == 1.0f);
    CHECK(loaded.ambienceVolume == 0.5f);
}

// --- Decisive tightens ----------------------------------------------------------

TEST_CASE("qol: Decisive now demands 15 turns or fewer", "[qol]") {
    const cd::Party party;
    cd::AchvContext ctx;
    ctx.clearedDungeon = true;
    ctx.runTurns = 15;
    CHECK(cd::achievementMet("decisive", party, ctx));
    ctx.runTurns = 16;
    CHECK_FALSE(cd::achievementMet("decisive", party, ctx));
    const cd::AchievementDef* def = cd::findAchievement("decisive");
    REQUIRE(def != nullptr);
    CHECK(std::string(def->description).find("15") != std::string::npos);
}
