#include "capture/CaptureRunner.hpp"

#ifdef CRYSTAL_CAPTURE

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "assets/AssetManifest.hpp"
#include "audio/AudioManager.hpp"
#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "core/GameConfig.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/TeamInspect.hpp"  // M88: describeTeam for the inspection scene
#include "dungeon/ThemeEvents.hpp"  // M87: eventFlavorId for the long-flavor scene
#include "game/Achievements.hpp"
#include "game/Profile.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "render/VirtualScreen.hpp"
#include "resource/ResourceManager.hpp"
#include "save/SaveSystem.hpp"
#include "score/ScoreEntry.hpp"
#include "score/Scoreboard.hpp"
#include "score/Scoring.hpp"
#include "settings/Settings.hpp"
#include "states/BattleLog.hpp"
#include "states/BattleLogState.hpp"
#include "states/BattleState.hpp"
#include "states/CelebrationState.hpp"  // M71
#include "states/BossIntroState.hpp"
#include "states/BlackMarketState.hpp"
#include "states/AchievementsState.hpp"
#include "states/BestiaryState.hpp"
#include "states/CastleChallengeState.hpp"
#include "states/CastleState.hpp"
#include "states/GooseTownState.hpp"
#include "states/GuildPerkChoiceState.hpp"
#include "states/MilestoneChoiceState.hpp"
#include "game/Curios.hpp"
#include "game/Story.hpp"  // M85: kDragonJesterBeat
#include "states/MapsState.hpp"
#include "states/PartyState.hpp"
#include "states/TreasureFightState.hpp"
#include "states/StoryDialogState.hpp"
#include "states/DetailsOverlayState.hpp"
#ifdef CRYSTAL_DEBUG_OVERLAY
#include "states/DebugMenuState.hpp"
#endif
#include "states/DungeonMenuState.hpp"
#include "states/DungeonResultState.hpp"
#include "states/DungeonState.hpp"
#include "states/BlackjackEventState.hpp"  // 2026-08-29: the card-table scene
#include "states/EventChoiceState.hpp"
#include "states/EquipShopState.hpp"
#include "states/InventoryState.hpp"  // M90
#include "states/ScrollChoiceState.hpp"  // M92
#include "states/SparState.hpp"  // M94
#include "states/CutsceneState.hpp"  // M97
#include "states/GuildState.hpp"
#include "states/HelpState.hpp"
#include "states/InnState.hpp"
#include "states/ItemShopState.hpp"
#include "states/MainMenuState.hpp"
#include "states/PartyCreationState.hpp"
#include "states/QuitFlow.hpp"
#include "states/QuitPrompt.hpp"
#include "states/RemapState.hpp"
#include "states/ScoreDetailsText.hpp"
#include "states/ScoreboardState.hpp"
#include "states/SettingsState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
#include "states/TownMenuState.hpp"
#include "states/TownState.hpp"
#include "states/TrainingHallState.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd::capture {

namespace fs = std::filesystem;

namespace {

// Mirrors Application's bundled-directory resolution (exe dir, then cwd).
fs::path bundledDir(const char* name) {
    const fs::path exeDir(GetApplicationDirectory());
    if (fs::exists(exeDir / name)) {
        return exeDir / name;
    }
    return fs::path(name);
}

struct Scenario {
    const char* name;
    std::function<void(StateStack&, AppContext&)> setup;
};

// Builds a 4-member party with maximum-length (12-char, wide-glyph) names —
// the worst case every name region must fit.
Party makeCaptureParty(const content::ContentDatabase& db) {
    Party party;
    const char* names[4] = {"WWWWWWWWWWWW", "MMMMMMMMMMMM", "Christabelle",
                            "Wolfgangheim"};
    std::vector<std::string> classIds;
    for (const auto& [id, def] : db.classes()) {
        (void)def;
        classIds.push_back(id);
    }
    std::sort(classIds.begin(), classIds.end());
    for (int i = 0; i < 4 && i < static_cast<int>(classIds.size()); ++i) {
        const content::ClassDef* cls = db.findClass(classIds[static_cast<std::size_t>(i)]);
        party.members.push_back(createCharacter(*cls, names[i], 7));
    }
    party.gold = 99999;
    return party;
}

// A five-enemy team from the shipped roster (sorted ids: deterministic).
dungeon::EnemyTeam makeFiveEnemyTeam(const content::ContentDatabase& db) {
    dungeon::EnemyTeam team;
    team.name = "Wardens of the Sunken Reliquary";  // longest plausible label
    std::vector<std::string> ids;
    for (const auto& [id, def] : db.enemies()) {
        if (def.tier == content::EnemyTier::Normal) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    for (int i = 0; i < 5 && i < static_cast<int>(ids.size()); ++i) {
        team.enemyIds.push_back(ids[static_cast<std::size_t>(i)]);
    }
    team.tags = {"Fast", "Magic", "Armored", "Poison"};
    team.statScalePct = 130;
    return team;
}

dungeon::EnemyTeam makeBossTeam(const content::ContentDatabase& db) {
    dungeon::EnemyTeam team;
    team.isBoss = true;
    std::vector<std::string> ids;
    for (const auto& [id, def] : db.bosses()) {
        (void)def;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    if (!ids.empty()) {
        team.bossId = ids.front();
        const content::BossDef* boss = db.findBoss(team.bossId);
        team.name = boss != nullptr ? boss->name : "Boss";
        if (boss != nullptr) {
            for (int i = 0; i < 2 && i < static_cast<int>(boss->minions.size()); ++i) {
                team.enemyIds.push_back(boss->minions[static_cast<std::size_t>(i)]);
            }
        }
    }
    return team;
}

// The Goosy Gauntlet's five new normals on their own stage (2026-08-17).
dungeon::EnemyTeam makeGoosyTeam(const content::ContentDatabase& db) {
    (void)db;
    dungeon::EnemyTeam team;
    team.name = "The Pond Patrol";
    team.enemyIds = {"pond_drake", "reed_honker", "mallard_marauder", "downfeather_witch",
                     "puddle_imp"};
    team.tags = {"Fast", "Magic"};
    team.statScalePct = 130;
    return team;
}

// One of the three new Goosy bosses with its authored court (2026-08-17).
dungeon::EnemyTeam makeGoosyBossTeam(const content::ContentDatabase& db) {
    dungeon::EnemyTeam team;
    team.isBoss = true;
    team.bossId = "the_pondlord";
    const content::BossDef* boss = db.findBoss(team.bossId);
    team.name = boss != nullptr ? boss->name : "Boss";
    if (boss != nullptr) {
        for (const std::string& minion : boss->minions) {
            team.enemyIds.push_back(minion);
        }
    }
    return team;
}

// Applies a spread of statuses so battle rows show every tag at once, including
// the M35 Confusion/Silence/Blind (duration-only, magnitude 0) and a stacked row
// to stress the status-line width with the wider labels.
void applyCaptureStatuses(battle::Battle& b) {
    using content::StatusType;
    int i = 0;
    for (battle::Combatant& u : b.units) {
        switch (i % 7) {
            case 0: u.statuses.push_back({StatusType::Poison, 5, 3}); break;
            case 1: u.statuses.push_back({StatusType::AttackUp, 25, 2}); break;
            case 2: u.statuses.push_back({StatusType::DefenseDown, 25, 2}); break;
            case 3: u.statuses.push_back({StatusType::Blind, 0, 3}); break;     // M35
            case 4: u.statuses.push_back({StatusType::Silence, 0, 3}); break;   // M35
            case 5: u.statuses.push_back({StatusType::Confusion, 0, 2}); break;  // M35
            default:
                u.statuses.push_back({StatusType::Poison, 3, 2});
                u.statuses.push_back({StatusType::DefenseUp, 25, 1});
                u.statuses.push_back({StatusType::Blind, 0, 2});  // widest: 3 stacked labels
                break;
        }
        ++i;
    }
}

score::RunSummary maximalRunSummary() {
    score::RunSummary run;
    run.completed = true;
    run.battleTurns = 87;
    run.dangerDefeated = 44;
    run.chestsOpened = 9;
    run.treasureGold = 1234;
    run.noDeath = false;  // shows the wager-lost line
    run.escapes = 3;
    run.wagerAccepted = true;
    run.townBonusPct = 100;      // M32: max town bonus
    run.stakesPenaltyPct = 99;   // M33/M35: max penalty (-99%) -> town-bonus + penalty rows, fullest panel
    run.classModPct = -80;       // M45: an all-Dragon party -> the class row, fullest panel
    return run;
}

// M87: deterministic pseudo-localization for the long-prose stress scenes —
// swaps ASCII vowels for their supported Latin-1 kin and appends ~a third of
// expansion padding, the classic translation-growth envelope. Capture-only:
// nothing here ever touches shipping content, and the swapped glyphs double
// as an in-situ render check of the M87 font extension.
std::string pseudoLocalize(const std::string& text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (const char ch : text) {
        switch (ch) {
            case 'a': out += "\xC3\xA5"; break;  // å
            case 'e': out += "\xC3\xA9"; break;  // é
            case 'i': out += "\xC3\xAF"; break;  // ï
            case 'o': out += "\xC3\xB8"; break;  // ø
            case 'u': out += "\xC3\xBC"; break;  // ü
            case 'A': out += "\xC3\x86"; break;  // Æ
            case 'O': out += "\xC3\x96"; break;  // Ö
            case 'U': out += "\xC3\x9C"; break;  // Ü
            default: out += ch; break;
        }
    }
    out += " \xC2\xAB";  // «
    for (std::size_t i = 0; i < text.size() / 3; i += 8) {
        out += " \xC3\xA6\xC3\xB0\xC3\x9F~";  // æðß~
    }
    out += " \xC2\xBB";  // »
    return out;
}

// M87: a pangram-style line exercising every family of the new Latin glyphs,
// appended to a long Details body so the atlas extension is visually pinned.
// (Escapes are split with "" wherever a hex digit follows, or MSVC's greedy
// \x parsing swallows it.)
const char* kLatinPangram =
    "\xC3\x86" "blegr\xC3\xB8" "d p\xC3\xA5 \xC3\x98" "rken\xC3\xB8" "y: "
    "\xC3\xA4\xC3\xB6\xC3\xBC \xC3\x84\xC3\x96\xC3\x9C \xC3\x9F, "
    "\xC3\xA7 \xC3\xA9\xC3\xA8\xC3\xAA\xC3\xAB \xC3\xA0\xC3\xA2 "
    "\xC3\xAD\xC3\xAC\xC3\xAE\xC3\xAF \xC3\xB3\xC3\xB2\xC3\xB4\xC3\xB5 "
    "\xC3\xBA\xC3\xB9\xC3\xBB \xC3\xB1 \xC3\xBD\xC3\xBF \xC3\xB0\xC3\xBE "
    "\xC2\xA1hola! \xC2\xBF" "qu\xC3\xA9? \xC2\xAB" "cit\xC3\xA9\xC2\xBB "
    "\xC3\x80\xC3\x82 \xC3\x89\xC3\x88\xC3\x8A\xC3\x8B \xC3\x87 "
    "\xC3\x91 \xC3\x8D\xC3\x8E \xC3\x93\xC3\x94 \xC3\x9A\xC3\x9B \xC3\x9D \xC3\x90\xC3\x9E.";

}  // namespace

int run(const char* outDir) {
    const fs::path out(outDir);
    fs::create_directories(out);
    const fs::path scratch = fs::temp_directory_path() / "crystal_capture_scratch";
    std::error_code ec;
    fs::remove_all(scratch, ec);
    fs::create_directories(scratch);

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(config::kVirtualWidth, config::kVirtualHeight, "ArePGeese capture");
    SetExitKey(KEY_NULL);
    SetRandomSeed(123456789u);  // GuildState's seed roll etc. stay fixed

    int failures = 0;
    int scenes = 0;
    {
        VirtualScreen screen(config::kVirtualWidth, config::kVirtualHeight);
        ResourceManager resources;
        content::ContentDatabase content;
        assets::AssetManifest manifest;
        {
            content::LoadReport report;
            if (!content::loadAll(bundledDir("data"), content, report)) {
                std::fprintf(stderr, "capture: content failed to load:\n%s\n",
                             report.summary().c_str());
                CloseWindow();
                return 2;
            }
            content::LoadReport assetReport;
            manifest.load(bundledDir("assets"), assetReport);
            // M87: one capture-only skill with a deliberately long description,
            // so the battle preview's EXPLICIT truncation (policy B) and the
            // full-sheet Details overlay are exercised by a real db entry. No
            // class, enemy, or generator pool references it, so every other
            // scene is byte-identical with or without it.
            content::LoadReport stretchReport;
            content::parseSkills(
                content::Json::parse(R"({"version":1,"skills":[{
                    "id":"zz_capture_stretch","name":"Winter Saga",
                    "category":"magic","target":"all_enemies","power":18,"mpCost":9,
                    "description":"A saga recited in full before the frost obeys: it names every winter the realm has endured, every duck that weathered them, and every excuse the court scribes offered for both. Foes caught listening take ice damage scaled by their patience, and translated editions of the saga only ever grow longer."
                }]})"),
                "capture_stretch", content, stretchReport);
        }
        const fs::path assetsRoot = bundledDir("assets");
        resources.setCatalog(&manifest, assetsRoot);
        resources.reload();
        // Validate overflow against the real bitmap font (M25), not the default.
        ui::setFonts(&resources.font("font.ui.small"), &resources.font("font.ui.main"),
                     &resources.font("font.ui.title"));

        Party party = makeCaptureParty(content);
        save::SaveSystem saves(content, scratch / "saves");
        score::Scoreboard scoreboard(scratch / "scoreboard.json");
        AudioManager audio;
        audio.setEnabled(false);  // captures are visual; keep runs silent
        FadeController fade;
        Input input;
        settings::SettingsStore settings(scratch / "settings.json");
        tutorial::TutorialStore tutorial(scratch / "tutorial.json");
        tutorial.state.enabled = false;  // prompts only in their own scene
        AchievementStore achievements(scratch / "achievements.json");
        // M45: captures run with the reward classes UNLOCKED unless a scene says
        // otherwise, so the class-select scene can show both states.
        ProfileStore profile(scratch / "profile.json");
        profile.data.kingDefeated = true;

        AppContext ctx{resources,  content,  saves,        party,
                       scoreboard, audio,    fade,         input,
                       settings,   tutorial, achievements, profile,
                       config::kVirtualWidth, config::kVirtualHeight};

        // One full save slot (the others stay empty) for the slot menu.
        {
            content::LoadReport report;
            saves.save(save::SaveSlot::Manual1, party, report);
        }
        // Scoreboard extremes: modern maximal, legacy dashes, and enough
        // rows to exercise scrolling.
        {
            score::ScoreEntry top;
            top.score = 999999;
            top.battleTurns = 999;
            top.dangerDefeated = 99;
            top.chestsOpened = 99;
            top.noDeath = true;
            top.depth = 20;
            top.theme = "Hollow Forest";
            top.seed = 18446744073709551615ull;  // widest seed
            top.generationVersion = 4;
            top.partyLevel = 50;
            top.battleRulesVersion = 1;
            top.townIndex = 7;  // M32: exercise the "T7" tag in the fitted theme column
            scoreboard.add(top);
            score::ScoreEntry legacy;
            legacy.score = 100;
            legacy.battleTurns = 12;
            legacy.depth = 1;
            legacy.theme = "Ruined Keep";
            legacy.seed = 7;
            scoreboard.add(legacy);  // generationVersion/partyLevel = 0 -> "-"
            for (int i = 0; i < 10; ++i) {
                score::ScoreEntry mid;
                mid.score = 5000 - i * 100;
                mid.battleTurns = 40 + i;
                mid.depth = 3 + i;
                mid.theme = "Crystal Mine";
                mid.seed = 1000u + static_cast<std::uint64_t>(i);
                mid.generationVersion = 4;
                mid.partyLevel = 5 + i;
                mid.battleRulesVersion = 1;
                scoreboard.add(mid);
            }
            // M82: a few 4-floor runs, so the 4F board renders rows (they are
            // invisible on the 1F board — the split itself under test).
            for (int i = 0; i < 3; ++i) {
                score::ScoreEntry deep;
                deep.score = 9000 - i * 500;
                deep.battleTurns = 120 + 10 * i;
                deep.dangerDefeated = 40 + i;
                deep.depth = 6 + i;
                deep.theme = "Hollow Forest";
                deep.seed = 4000u + static_cast<std::uint64_t>(i);
                deep.generationVersion = 15;
                deep.partyLevel = 30 + i;
                deep.battleRulesVersion = 15;
                deep.townIndex = 4;
                deep.floors = 4;
                scoreboard.add(deep);
            }
        }

        battle::BattleResult battleSlot;  // outlives the battle scenes

        // M99: prompts show the forge-authored text (data/tutorials.json) with
        // the constexpr beat as fallback — so the capture referees the longest
        // RESOLVED body, exactly what a player can be shown.
        const auto longestResolvedBeat = [](const AppContext& c) {
            std::string title = tutorial::kBeats[0].title;
            std::string body = tutorial::kBeats[0].body;
            for (const tutorial::Beat& b : tutorial::kBeats) {
                std::string t = b.title;
                std::string bd = b.body;
                if (const content::TutorialTextDef* d = c.content.findTutorialText(b.id)) {
                    t = d->title;
                    bd = d->body;
                }
                if (bd.size() > body.size()) {
                    title = std::move(t);
                    body = std::move(bd);
                }
            }
            return std::pair<std::string, std::string>(std::move(title), std::move(body));
        };

        const std::vector<Scenario> scenarios = {
            {"01_title",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<MainMenuState>(s, c));
             }},
            {"02_help",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<HelpState>(s, c));
             }},
            {"03_settings",
             [](StateStack& s, AppContext& c) {
                 // M51: the top-level category picker (Audio / Display / ...).
                 s.pushState(std::make_unique<SettingsState>(s, c));
             }},
            {"60_settings_display",
             [](StateStack& s, AppContext& c) {
                 // M51: the Display submenu, showing the CRT Effect toggle and the
                 // effect/contrast rows.
                 auto st = std::make_unique<SettingsState>(s, c);
                 st->captureShowDisplay();
                 s.pushState(std::move(st));
             }},
            {"04_remap_keyboard",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<RemapState>(s, c, ActiveDevice::Keyboard));
             }},
            {"05_party_creation",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<PartyCreationState>(s, c));
             }},
            {"06_town",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<TownState>(s, c));
             }},
            {"07_inn",
             [](StateStack& s, AppContext& c) {
                 c.party.restTokens = 1;  // exercise the token row for overflow (M30)
                 s.pushState(std::make_unique<TownState>(s, c));
                 s.pushState(std::make_unique<InnState>(s, c));
             }},
            {"08_item_shop",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<ItemShopState>(s, c));
             }},
            {"09_equip_shop",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<EquipShopState>(s, c));
             }},
            {"24_equip_categories",
             [](StateStack& s, AppContext& c) {
                 // M31: open straight into the buy-category menu (Weapons /
                 // Armor / Accessories) so the new category UI is overflow-checked.
                 auto state = std::make_unique<EquipShopState>(s, c);
                 state->captureEnterBuyCategory();
                 s.pushState(std::move(state));
             }},
            {"10_training_hall",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<TrainingHallState>(s, c));
             }},
            {"122_training_spar_row",
             [](StateStack& s, AppContext& c) {
                 // Regression (2026-08-17): the cursor on the first spar row —
                 // this exact hover frame indexed members[] out of range and
                 // crashed debug builds before the fix.
                 auto state = std::make_unique<TrainingHallState>(s, c);
                 state->captureHoverSparRow();
                 s.pushState(std::move(state));
             }},
            {"123_spar_battle",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // Owner fix 2026-08-17: the echoes wear the party's own class
                 // sprites (flipped to face their originals), not the generic
                 // enemy beast.
                 s.pushState(std::make_unique<BattleState>(
                     s, c, battle::buildSparBattle(c.party, c.content), &battleSlot));
             }},
            {"124_event_choice_scroll",
             [](StateStack& s, AppContext& c) {
                 // Owner fix 2026-08-17: a deep-bag Sacrifice list once grew
                 // the modal past the screen — the rows now scroll in a fixed
                 // window under a two-line title.
                 s.pushState(std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 8, c.content, "crystal_mine")));
                 std::vector<std::string> rows;
                 for (int i = 1; i <= 20; ++i) {
                     rows.push_back(TextFormat("Dawnforged Blade  x%d", i));
                 }
                 s.pushState(std::make_unique<EventChoiceState>(
                     s, c,
                     "Feed the forge one piece - the next battle pays double experience.",
                     std::move(rows), [](int) {}));
             }},
            {"11_slot_menu_save",
             [](StateStack& s, AppContext& c) {
                 // Write one occupied slot carrying the King title, the widest
                 // row content the screen can show.
                 const int gold = c.party.gold;
                 c.party.gold = 999999;
                 c.party.castleRecords.kingTitle = kKingTitle;
                 content::LoadReport report;
                 c.saves.save(save::SaveSlot::Manual1, c.party, report);
                 c.party.gold = gold;  // the slot summary keeps it; later scenes must not
                 c.party.castleRecords.kingTitle.clear();
                 s.pushState(std::make_unique<SlotMenuState>(s, c, SlotMenuMode::Save));
             }},
            {"12_scoreboard",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<ScoreboardState>(s, c));
             }},
            {"13_guild",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<GuildState>(s, c));
             }},
            {"106_guild_seed_edit",
             [](StateStack& s, AppContext& c) {
                 // M88: the manual seed editor with a 20-digit buffer — the
                 // widest text the modal and the Seed row capsule can face.
                 auto st = std::make_unique<GuildState>(s, c);
                 st->captureOpenSeedEditor();
                 s.pushState(std::move(st));
             }},
            {"108_items_bag",
             [](StateStack& s, AppContext& c) {
                 // M90: the pause menus' Items screen over a representative
                 // bag — every band (consumables in the M88 order, gear,
                 // scroll), counts, and the longest detail preview.
                 c.party.inventory.add("potion", 3);
                 c.party.inventory.add("ether", 2);
                 c.party.inventory.add("antidote", 1);
                 c.party.inventory.add("phoenix_tear", 1);
                 c.party.inventory.add("holy_taxes", 1);
                 c.party.inventory.add("worldbreaker_axe", 1);
                 c.party.inventory.add("scroll_fireball", 1);
                 auto st = std::make_unique<InventoryState>(s, c);
                 st->captureCursorToItem("holy_taxes");  // longest description
                 s.pushState(std::move(st));
             }},
            {"109_equip_party",
             [](StateStack& s, AppContext& c) {
                 // M90: the shop's equip flow in party mode — "Equip Party"
                 // header, member list, no shop rows.
                 s.pushState(std::make_unique<EquipShopState>(s, c, /*partyMode=*/true));
             }},
            {"112_cutscene_dialogue",
             [](StateStack& s, AppContext& c) {
                 // M97: a prologue beat — party lineup, the hooded goose on
                 // stage, and the dialogue panel over the longest early beat
                 // (beat 3 carries the flightways paragraph + waddle emote).
                 auto st = std::make_unique<CutsceneState>(s, c, "new_game", /*replay=*/true);
                 st->captureShowBeat(3);
                 s.pushState(std::move(st));
             }},
            {"113_cutscene_choice",
             [](StateStack& s, AppContext& c) {
                 // M97: the mandatory choice modal — question preview, two
                 // option rows, and the highlighted keepsake's description.
                 auto st = std::make_unique<CutsceneState>(s, c, "new_game", /*replay=*/true);
                 st->captureShowChoice();
                 s.pushState(std::move(st));
             }},
            {"114_cutscene_finale",
             [](StateStack& s, AppContext& c) {
                 // M97: the finale's fullest stage — King AND Dragon staged
                 // behind the goose (beat 2, the king's telling).
                 auto st = std::make_unique<CutsceneState>(s, c, "finale", /*replay=*/true);
                 st->captureShowBeat(2);
                 s.pushState(std::move(st));
             }},
            {"111_spar_closing",
             [](StateStack& s, AppContext& c) {
                 // M94: the sparring hall's closing modal, longest line.
                 auto st = std::make_unique<SparState>(s, c, /*manual=*/false);
                 st->captureShowClosing();
                 s.pushState(std::move(st));
             }},
            {"110_scroll_trove",
             [](StateStack& s, AppContext& c) {
                 // M92: the Guild's trove modal with the longest offer names +
                 // the two-line description preview under the list.
                 s.pushState(std::make_unique<ScrollChoiceState>(
                     s, c,
                     std::vector<std::string>{"scroll_piercing_arrow", "scroll_group_mend",
                                              "scroll_battle_cry"}));
             }},
            {"107_team_inspect",
             [](StateStack& s, AppContext& c) {
                 // M88: the pre-fight inspection body for a generated boss team
                 // with its court — the fullest roster the overlay renders,
                 // through the same describeTeam the dungeon Details uses.
                 const dungeon::Dungeon d =
                     dungeon::generate(424242, 12, c.content, "ruined_keep", 7);
                 const dungeon::EnemyTeam* team = nullptr;
                 for (const dungeon::EnemyTeam& t : d.teams) {
                     if (t.isBoss) {
                         team = &t;
                     }
                 }
                 s.pushState(std::make_unique<DetailsOverlayState>(
                     s, c, "Dungeon Details",
                     team != nullptr ? dungeon::describeTeam(*team, "Boss", c.content)
                                     : std::string("generation yielded no boss team")));
             }},
            {"89_scoreboard_4f",
             [](StateStack& s, AppContext& c) {
                 // M82: the 4-floor board — its chip, rows, and cycle hint.
                 auto st = std::make_unique<ScoreboardState>(s, c);
                 st->captureShowFourFloorBoard();
                 s.pushState(std::move(st));
             }},
            {"90_dungeon_stairs",
             [](StateStack& s, AppContext& c) {
                 // M82: floor 1 of a 4-floor run with its stair-gate cleared —
                 // the opened stairway marker, the descend prompt, and the
                 // F1/4 chip are all overflow-checked here.
                 auto st = std::make_unique<DungeonState>(
                     s, c,
                     dungeon::generateFloors(424242, 6, c.content, "ruined_keep", 1, 4));
                 st->captureOpenStairs();
                 s.pushState(std::move(st));
             }},
            {"92_guild_boss_locked",
             [](StateStack& s, AppContext& c) {
                 // M84: the Guild Boss row while the audience is unearned — the
                 // dim row plus the longest status banner (the lock hint).
                 auto st = std::make_unique<GuildState>(s, c);
                 st->captureFocusGuildBoss();
                 s.pushState(std::move(st));
             }},
            {"93_guild_boss_best",
             [](StateStack& s, AppContext& c) {
                 // M84: the same row after a victory — the gold best-turns
                 // readout and the rematch banner with its %d expansion.
                 guildRecord(c.party.guild, c.party.currentTown).unlocked = true;
                 guildRecord(c.party.guild, c.party.currentTown).bestTurns = 888;
                 auto st = std::make_unique<GuildState>(s, c);
                 st->captureFocusGuildBoss();
                 s.pushState(std::move(st));
             }},
            {"94_guild_perk",
             [](StateStack& s, AppContext& c) {
                 // M84: the town-milestone modal on town 5 — the cryptic
                 // Mind-the-Spoon description is the longest option text.
                 auto st = std::make_unique<GuildPerkChoiceState>(s, c);
                 st->captureSelect(5);
                 s.pushState(std::move(st));
             }},
            {"95_guild_result",
             [](StateStack& s, AppContext& c) {
                 // M84: the gauntlet's fullest first-victory overlay (longest
                 // Master name + the milestone invitation, wrapped).
                 auto st = std::make_unique<CastleChallengeState>(
                     s, c, CastleChallenge::GuildBoss, 6);
                 st->captureGuildResult();
                 s.pushState(std::move(st));
             }},
            {"14_dungeon_keep",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 6, c.content, "ruined_keep")));
             }},
            {"15_dungeon_mine",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 8, c.content, "crystal_mine")));
             }},
            {"16_dungeon_forest",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 10, c.content, "hollow_forest")));
             }},
            {"50_dungeon_pause",
             [](StateStack& s, AppContext& c) {
                 // M46: the pause modal over a live dungeon — one of the six
                 // facelift acceptance screens (presentation-only hook).
                 s.pushState(std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 8, c.content, "crystal_mine")));
                 s.pushState(std::make_unique<DungeonMenuState>(s, c));
             }},
            {"17_battle_five_enemies",
             [&battleSlot](StateStack& s, AppContext& c) {
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 applyCaptureStatuses(b);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot));
             }},
            {"18_battle_boss",
             [&battleSlot](StateStack& s, AppContext& c) {
                 battle::Battle b =
                     battle::buildBattle(c.party, makeBossTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot));
             }},
            {"117_summon_goose",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M107: the Mighty G. Goose's apparition frozen mid-beat over a
                 // five-enemy field — the stagecraft's composition check.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureShowSummon("summon_goose");
                 s.pushState(std::move(state));
             }},
            {"118_dungeon_goosy",
             [](StateStack& s, AppContext& c) {
                 // Owner direction 2026-08-17: the Goosy Gauntlet's own tiles.
                 s.pushState(std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 20, c.content, "goosy_gauntlet", 7)));
             }},
            {"119_reels_icons",
             [](StateStack& s, AppContext& c) {
                 // The reels outcome with its icon rows over the panel text.
                 auto state = std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 8, c.content, "crystal_mine"));
                 state->captureShowReels();
                 s.pushState(std::move(state));
             }},
            {"120_battle_goosy",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // The five new pond-fowl normals on the Goosy reed stage.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeGoosyTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(
                     s, c, std::move(b), &battleSlot, MusicTrack::None, nullptr,
                     /*castleChallenge=*/false, render::BackdropStage::Goosy));
             }},
            {"121_battle_goosy_boss",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // The Pondlord and its court — the 36x36 boss canvas proof.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeGoosyBossTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(
                     s, c, std::move(b), &battleSlot, MusicTrack::None, nullptr,
                     /*castleChallenge=*/false, render::BackdropStage::Goosy));
             }},
            {"23_battle_targeting",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // Drive the battle into target selection so the M25 target-info
                 // panel (name, vitals, judgment stats, statuses) is covered by
                 // the overflow check with maximal content.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 applyCaptureStatuses(b);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureEnterTargeting();
                 s.pushState(std::move(state));
             }},
            {"19_result",
             [](StateStack& s, AppContext& c) {
                 const score::RunSummary run = maximalRunSummary();
                 s.pushState(std::make_unique<DungeonResultState>(
                     s, c, run, score::computeScore(run)));
             }},
            {"20_tutorial_prompt",
             [longestResolvedBeat](StateStack& s, AppContext& c) {
                 auto [title, body] = longestResolvedBeat(c);  // M99
                 s.pushState(std::make_unique<TownState>(s, c));
                 s.pushState(std::make_unique<TutorialPromptState>(
                     s, c, std::move(title), std::move(body)));
             }},
            {"21_details_scoring",
             [](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<ScoreboardState>(s, c));
                 s.pushState(std::make_unique<DetailsOverlayState>(
                     s, c, "How Scoring Works", scoreDetailsText()));
             }},
            {"22_town_high_contrast",
             [](StateStack& s, AppContext& c) {
                 ui::style::setHighContrast(true);
                 s.pushState(std::make_unique<TownState>(s, c));
             }},
            {"25_town_ladder",
             [](StateStack& s, AppContext& c) {
                 // M32: a mid-ladder town shows the per-town exterior palette, the
                 // Town n/7 indicator, and both exit signposts (previous unlocked,
                 // next still locked). Placed last so the town-index mutation does
                 // not leak into the town-1 scenes above.
                 c.party.currentTown = 6;
                 c.party.highestUnlockedTown = 6;  // next (town 7) exit reads locked
                 c.party.blackMarket = {true, 6, "dawnforged_blade", 6500, 14, 6};  // M34 NPC
                 s.pushState(std::make_unique<TownState>(s, c));
             }},
            {"59_town_road",
             [](StateStack& s, AppContext& c) {
                 // M50: the walk-through road affordance — the player parked on an
                 // unlocked west road trigger, with the "Walk out to Town N"
                 // signpost and footer. Placed after the ladder scene; the town
                 // mutation is set explicitly so nothing leaks in.
                 c.party.currentTown = 3;
                 c.party.highestUnlockedTown = 3;
                 c.party.blackMarket = {};  // no NPC in this shot
                 auto town = std::make_unique<TownState>(s, c);
                 town->captureStandAtWestExit();
                 s.pushState(std::move(town));
             }},
            {"26_guild_penalty",
             [](StateStack& s, AppContext& c) {
                 // M33: a Guild whose configured run does not raise the stakes,
                 // so the forewarning shows a penalty. Placed after the town
                 // scenes; the stakes mutation does not leak into earlier scenes.
                 c.party.currentTown = 1;
                 // M88: pinned at the -99% cap (steps at the M35 ceiling) — the
                 // longest penalty text, now on the raised banner position.
                 c.party.stakes = {1, 20, 4};
                 s.pushState(std::make_unique<GuildState>(s, c));
             }},
            {"27_black_market",
             [](StateStack& s, AppContext& c) {
                 // M34: the purchase screen with the longest legendary name +
                 // description and an affordable token row, to overflow-check the
                 // stat/description regions.
                 c.party.legendaryTokens = 3;
                 c.party.blackMarket = {true, 1, "titanforged_heart", 8750, 16, 6};
                 s.pushState(std::make_unique<BlackMarketState>(s, c));
             }},
            {"28_training_passives",
             [](StateStack& s, AppContext& c) {
                 // M36: the Training Hall passive-management screen, with a mix of
                 // equipped / owned / priced rows to overflow-check the list.
                 c.party.gold = 5000;
                 c.party.members[0].ownedPassives = {"counter_attack", "evasion"};
                 c.party.members[0].equippedPassive = "evasion";
                 auto st = std::make_unique<TrainingHallState>(s, c);
                 st->captureEnterPassives();
                 s.pushState(std::move(st));
             }},
            {"29_battle_passive_reveal",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M36: target-info reveals a foe's passive (troll_berserker carries
                 // Counter Attack; shadow_stalker carries Evasion).
                 dungeon::EnemyTeam team;
                 team.name = "Berserker Vanguard";
                 team.enemyIds = {"troll_berserker", "shadow_stalker"};
                 battle::Battle b = battle::buildBattle(c.party, team, c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureEnterTargeting();
                 s.pushState(std::move(state));
             }},
            {"30_equip_shop_max",
             [](StateStack& s, AppContext& c) {
                 // M37: the buy list at town 7 (max stock) with the per-town gear
                 // and longest names, to overflow-check the scrolling list.
                 c.party.currentTown = 7;
                 c.party.gold = 9999;
                 auto state = std::make_unique<EquipShopState>(s, c);
                 state->captureEnterBuyList(content::EquipSlot::Weapon);
                 s.pushState(std::move(state));
             }},
            {"88_ward_charms",
             [](StateStack& s, AppContext& c) {
                 // M81: the accessory buy list where the ward-charm set lives,
                 // cursor parked on a charm so its resist detail line and the
                 // gear-icon column are both overflow-checked.
                 c.party.currentTown = 7;
                 c.party.gold = 9999;
                 auto state = std::make_unique<EquipShopState>(s, c);
                 state->captureEnterBuyList(content::EquipSlot::Accessory);
                 state->captureCursorToItem("stoneward_charm");
                 s.pushState(std::move(state));
             }},
            {"31_battle_high_town",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M38: a five-enemy team of new town-7 foes (their own sprites),
                 // town-7 scaled, with live statuses - showcases + overflow-checks.
                 dungeon::EnemyTeam team;
                 team.name = "Vanguard of the Dread Sovereign";
                 team.enemyIds = {"titan_guard", "archon_of_ruin", "dread_knight",
                                  "soul_render", "void_stalker"};
                 team.statScalePct = 300;  // town-7-scale
                 battle::Battle b = battle::buildBattle(c.party, team, c.content);
                 applyCaptureStatuses(b);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot));
             }},
            {"32_result_drops",
             [](StateStack& s, AppContext& c) {
                 // M39: the fullest score breakdown AND a maximal boss drop (2
                 // tokens + the longest legendary name), to overflow-check the
                 // drop block appended below the breakdown.
                 const score::RunSummary run = maximalRunSummary();
                 BossDropResult drops;
                 drops.tokens = 2;                         // town-7 double
                 drops.legendary = true;
                 drops.legendaryId = "titanforged_heart";  // longest legendary name
                 s.pushState(std::make_unique<DungeonResultState>(
                     s, c, run, score::computeScore(run), drops));
             }},
            {"91_result_map",
             [](StateStack& s, AppContext& c) {
                 // M83: the fullest breakdown + drops PLUS the longest map-drop
                 // line (the banked IOU wording at 3), so the panel's tightened
                 // pitch and the wrapped gold line are overflow-checked.
                 const score::RunSummary run = maximalRunSummary();
                 BossDropResult drops;
                 drops.tokens = 2;
                 drops.legendary = true;
                 drops.legendaryId = "titanforged_heart";
                 s.pushState(std::make_unique<DungeonResultState>(
                     s, c, run, score::computeScore(run), drops, RunStats{},
                     "The guild owes you a map piece - dig up the treasure to collect "
                     "(3 banked)."));
             }},
            {"33_castle_hub",
             [](StateStack& s, AppContext& c) {
                 // M40: the castle throne hall with a full records panel (earned
                 // title) to overflow-check the hub layout. M85: the panel grew
                 // the Dragon row and the menu the Dragon option — fullest here.
                 c.party.castleUnlocked = true;
                 c.party.castleRecords.bossRushBestTurns = 44;
                 c.party.castleRecords.endlessBestWave = 17;
                 c.party.castleRecords.kingDefeated = true;
                 c.party.castleRecords.kingBestTurns = 18;
                 c.party.castleRecords.kingTitle = kKingTitle;
                 c.party.castleRecords.dragonBestTurns = 41;  // M85
                 s.pushState(std::make_unique<CastleState>(s, c));
             }},
            {"96_curio_lore",
             [](StateStack& s, AppContext& c) {
                 // M85: the Maps screen's inspect panel on the longest lore
                 // entry, with the whole collection owned so the grid shows
                 // every name under the cursor styling.
                 for (const CurioDef& cd : kCurios) {
                     c.party.ownedCurios.push_back(cd.id);
                 }
                 auto st = std::make_unique<MapsState>(s, c);
                 st->captureInspect(0);  // Crown Shard: the longest body
                 s.pushState(std::move(st));
             }},
            {"97_dragon_jester",
             [](StateStack& s, AppContext& c) {
                 // M85: the Pale Jester's introduction — the longest new dialog
                 // body, refereed in the story panel it actually uses.
                 if (const content::StoryBeat* beat =
                         c.content.findStoryBeat(kDragonJesterBeat)) {
                     s.pushState(std::make_unique<StoryDialogState>(
                         s, c, beat->speaker, beat->title, beat->body));
                 }
             }},
            {"34_king_battle",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M40: the King fight (its own sprite + telegraph), the hardest
                 // battle in the game. The King is dealt all three control statuses
                 // it is immune to -> they must NOT show as labels (display fix).
                 // M49: he now fights with his court, so the statuses go on the
                 // BOSS only — the guards are not immune, and labelling them too
                 // would bury the very thing this scene exists to check.
                 battle::Battle b = battle::buildBattle(c.party, kingTeam(c.content), c.content);
                 for (battle::Combatant& u : b.units) {
                     if (u.side == battle::Side::Enemy && u.isBoss) {
                         u.statuses.push_back({content::StatusType::Blind, 0, 4});
                         u.statuses.push_back({content::StatusType::Silence, 0, 4});
                         u.statuses.push_back({content::StatusType::Confusion, 0, 4});
                     }
                 }
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                           MusicTrack::KingBattle));
             }},
            {"51_battle_high_contrast",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M46: the densest battle layout under the high-contrast
                 // palette — meters, statuses, focus, and the boss frame must
                 // keep their shape distinctions, not merely recolor.
                 ui::style::setHighContrast(true);
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 applyCaptureStatuses(b);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureEnterTargeting();
                 s.pushState(std::move(state));
             }},
            {"35_castle_result",
             [](StateStack& s, AppContext& c) {
                 // M40: the challenge result overlay with the longest reward text
                 // (a King first-clear), to overflow-check the panel.
                 auto st = std::make_unique<CastleChallengeState>(s, c, CastleChallenge::King);
                 st->captureKingReward();
                 s.pushState(std::move(st));
             }},
            {"36_story_dialog",
             [](StateStack& s, AppContext& c) {
                 // M41: the storyteller's dialog overlay over a town, to overflow-
                 // check the wrapped-text panel.
                 s.pushState(std::make_unique<TownState>(s, c));
                 if (const content::StoryBeat* b = c.content.findStoryBeat(6)) {
                     s.pushState(std::make_unique<StoryDialogState>(s, c, b->speaker, b->title,
                                                                    b->body));
                 }
             }},
            {"37_jester",
             [](StateStack& s, AppContext& c) {
                 // M41: the Jester's punchline (the longest story beat), over the
                 // castle hub.
                 c.party.castleUnlocked = true;
                 s.pushState(std::make_unique<CastleState>(s, c));
                 if (const content::StoryBeat* b = c.content.findStoryBeat(kCastleTown)) {
                     s.pushState(std::make_unique<StoryDialogState>(s, c, b->speaker, b->title,
                                                                    b->body));
                 }
             }},
            {"38_bestiary",
             [](StateStack& s, AppContext& c) {
                 // M42: the bestiary over the full roster (undiscovered foes read
                 // as unknowns), to overflow-check the list and the detail panel.
                 int n = 0;
                 for (const auto& [id, def] : c.content.enemies()) {
                     (void)def;
                     c.party.encountered.push_back(id);
                     if (++n >= 8) break;
                 }
                 n = 0;
                 for (const auto& [id, def] : c.content.bosses()) {
                     (void)def;
                     c.party.encountered.push_back(id);
                     if (++n >= 4) break;
                 }
                 s.pushState(std::make_unique<BestiaryState>(s, c));
             }},
            {"39_achievements",
             [](StateStack& s, AppContext& c) {
                 // M42: the achievements screen with a mix of unlocked / locked.
                 c.achievements.unlocked = {"first_clear", "trailblazer", "kingslayer",
                                            "loremaster", "deep_diver"};
                 s.pushState(std::make_unique<AchievementsState>(s, c));
             }},
            {"40_run_stats",
             [](StateStack& s, AppContext& c) {
                 // M42: the run-stats Details overlay (result-screen Details key).
                 const std::string body =
                     "This run\nTotal damage dealt: 4820\nBiggest single hit: 612\n"
                     "Statuses inflicted: 7\nMVP: Christabelle Wolfgangheim\n\n"
                     "Personal records\nBiggest hit ever: 999\nMost damage in a run: 12345";
                 s.pushState(std::make_unique<DetailsOverlayState>(s, c, "Run Stats", body));
             }},
            {"41_bestiary_king",
             [](StateStack& s, AppContext& c) {
                 // The heaviest bestiary entry: the longest boss name, three
                 // passives on their own lines, and the longest flavor text.
                 c.party.encountered.push_back(kKingBossId);
                 c.party.encountered.push_back("obsidian_colossus");
                 auto state = std::make_unique<BestiaryState>(s, c);
                 state->captureSelect(kKingBossId);
                 s.pushState(std::move(state));
             }},
            {"42_battle_skills",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // The skill list with the widest party skill names, so the name
                 // column and the right-aligned MP column are both checked.
                 std::vector<std::string> skills;
                 for (const auto& [id, def] : c.content.classes()) {
                     (void)id;
                     skills.insert(skills.end(), def.startingSkills.begin(),
                                   def.startingSkills.end());
                     for (const content::LearnEntry& e : def.learnset) {
                         skills.push_back(e.skill);
                     }
                 }
                 std::sort(skills.begin(), skills.end(), [&c](const std::string& a,
                                                              const std::string& b) {
                     const content::SkillDef* sa = c.content.findSkill(a);
                     const content::SkillDef* sb = c.content.findSkill(b);
                     const std::size_t la = sa != nullptr ? sa->name.size() : 0;
                     const std::size_t lb = sb != nullptr ? sb->name.size() : 0;
                     return la != lb ? la > lb : a < b;
                 });
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureEnterSkillMenu(std::move(skills));
                 s.pushState(std::move(state));
             }},
            {"43_slot_menu_load",
             [](StateStack& s, AppContext& c) {
                 // The deepest slot list (M53): six occupied rows (autosave +
                 // five manual), each carrying a title line, plus a message under
                 // them.
                 const int gold = c.party.gold;
                 c.party.gold = 999999;
                 c.party.castleRecords.kingTitle = kKingTitle;
                 content::LoadReport report;
                 for (save::SaveSlot slot :
                      {save::SaveSlot::Auto, save::SaveSlot::Manual1, save::SaveSlot::Manual2,
                       save::SaveSlot::Manual3, save::SaveSlot::Manual4, save::SaveSlot::Manual5}) {
                     c.saves.save(slot, c.party, report);
                 }
                 c.party.gold = gold;
                 c.party.castleRecords.kingTitle.clear();
                 s.pushState(std::make_unique<SlotMenuState>(s, c, SlotMenuMode::Load));
             }},
            {"45_relic_event",
             [](StateStack& s, AppContext& c) {
                 // M44: a reliquary room with the player facing it, so the event's
                 // footer prompt is checked in situ. The seed is searched rather
                 // than pinned: a rare event's seed would rot at the next
                 // generation bump.
                 for (std::uint64_t seed = 1; seed < 4000; ++seed) {
                     dungeon::Dungeon d = dungeon::generate(seed, 20, c.content, "ruined_keep", 7);
                     bool holdsRelic = false;
                     for (const dungeon::Room& r : d.rooms) {
                         holdsRelic = holdsRelic ||
                                      r.event.kind == dungeon::RoomEventKind::RoyalRelic;
                     }
                     if (!holdsRelic) {
                         continue;
                     }
                     auto state = std::make_unique<DungeonState>(s, c, std::move(d));
                     if (state->captureFaceEvent(dungeon::RoomEventKind::RoyalRelic)) {
                         s.pushState(std::move(state));
                         return;
                     }
                 }
             }},
            {"86_event_flavor",
             [](StateStack& s, AppContext& c) {
                 // M80: the centered flavor panel over a live dungeon, opened on
                 // the DUCK PEDDLER — the longest authored body — so the wrap
                 // budget is refereed at maximum length (seed searched, as the
                 // relic scene does).
                 for (std::uint64_t seed = 1; seed < 4000; ++seed) {
                     dungeon::Dungeon d =
                         dungeon::generate(seed, 8, c.content, "crystal_mine", 3);
                     bool holdsPeddler = false;
                     for (const dungeon::Room& r : d.rooms) {
                         holdsPeddler = holdsPeddler ||
                                        r.event.kind == dungeon::RoomEventKind::DuckPeddler;
                     }
                     if (!holdsPeddler) {
                         continue;
                     }
                     auto state = std::make_unique<DungeonState>(s, c, std::move(d));
                     if (state->captureOpenEventPanel(dungeon::RoomEventKind::DuckPeddler)) {
                         s.pushState(std::move(state));
                         return;
                     }
                 }
             }},
            {"87_event_outcome",
             [](StateStack& s, AppContext& c) {
                 // M80 addendum: the outcome panel at a representative long
                 // result (the trapped chest's bite + loot is the widest
                 // dynamic outcome string).
                 auto state = std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 8, c.content, "crystal_mine"));
                 state->captureShowOutcome(
                     "The Chest",
                     "The trap bites - the party is wounded! Found 1240 gold + Hi-Potion");
                 s.pushState(std::move(state));
             }},
            {"46_battle_relics",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M44: the battle item list holding all four relics plus the
                 // snacks — the widest item names and the count column.
                 for (const char* id : {"evil_goose", "tax_sheets", "dragon_crown",
                                        "deadly_spoon", "royal_snacks"}) {
                     c.party.inventory.add(id, 2);
                 }
                 battle::Battle b =
                     battle::buildBattle(c.party, kingTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                            MusicTrack::KingBattle, nullptr, true);
                 state->captureEnterItemMenu();
                 s.pushState(std::move(state));
             }},
            {"47_class_select_locked",
             [](StateStack& s, AppContext& c) {
                 // M45: the reward classes are listed but locked, with the hint —
                 // the goal is visible from the very first New Game.
                 c.profile.data.kingDefeated = false;
                 auto state = std::make_unique<PartyCreationState>(s, c);
                 state->captureSelectClass(0, "dragon");
                 state->captureSelectClass(1, "jester");
                 state->captureSelectClass(2, "goose");
                 s.pushState(std::move(state));
             }},
            {"48_class_select_unlocked",
             [](StateStack& s, AppContext& c) {
                 c.profile.data.kingDefeated = true;  // after the King has fallen
                 auto state = std::make_unique<PartyCreationState>(s, c);
                 state->captureSelectClass(0, "dragon");
                 state->captureSelectClass(1, "jester");
                 state->captureSelectClass(2, "goose");
                 s.pushState(std::move(state));
             }},
            {"49_battle_jest",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M45: the longest quip, mid-screen, over a real battle.
                 Party jesters = c.party;
                 if (const content::ClassDef* cls = c.content.findClass("jester")) {
                     jesters.members.clear();
                     jesters.members.push_back(createCharacter(*cls, "Christabelle", 30));
                     for (int i = 1; i < 4; ++i) {
                         jesters.members.push_back(createCharacter(*cls, "Wolfgangheim", 30));
                     }
                 }
                 battle::Battle b =
                     battle::buildBattle(jesters, makeFiveEnemyTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureShowJest();
                 s.pushState(std::move(state));
             }},
            {"44_quit_confirm",
             [](StateStack& s, AppContext& c) {
                 // The pause menu's quit question, over the pause panel it opens
                 // from. M47: three answers, raised through the same helper the
                 // menu uses, so the scene cannot drift from the game.
                 s.pushState(std::make_unique<TownState>(s, c));
                 s.pushState(std::make_unique<TownMenuState>(s, c));
                 pushQuitPrompt(s, c, quit::kTownBody);
             }},
            {"61_aoe_tint",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M51: an all-enemies spell frozen at its impact beat, so the
                 // faint AoE screen tint is captured as the game draws it.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureAoeImpact("radiance");  // all_enemies holy damage -> danger tint
                 s.pushState(std::move(state));
             }},
            {"53_battle_weak_hit",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M48: a real fire hit on the fire-weak Frost Monarch — the
                 // "Weak!" float, and the target panel's affinity chips behind it.
                 dungeon::EnemyTeam team;
                 team.bossId = "frost_monarch";
                 team.statScalePct = 100;
                 battle::Battle b = battle::buildBattle(c.party, team, c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 if (const content::SkillDef* fireball = c.content.findSkill("fireball")) {
                     state->captureElementHit(*fireball);
                 }
                 s.pushState(std::move(state));
             }},
            {"54_battle_immune_hit",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M48: the same foe, hit with the ice it is immune to — the
                 // "Immune" float, which is the ONLY thing that marks a hit that
                 // moved no HP at all.
                 dungeon::EnemyTeam team;
                 team.bossId = "frost_monarch";
                 team.statScalePct = 100;
                 battle::Battle b = battle::buildBattle(c.party, team, c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 if (const content::SkillDef* blizzard = c.content.findSkill("blizzard")) {
                     state->captureElementHit(*blizzard);
                 }
                 s.pushState(std::move(state));
             }},
            {"57_kings_court",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M49: the King as he is now fought — flanked by both Royal
                 // Guards. Three enemy rows plus the party, so the battle
                 // layout is overflow-checked at the court's width.
                 battle::Battle b = battle::buildBattle(c.party, kingTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                            MusicTrack::KingBattle);
                 state->captureEnterTargeting();
                 s.pushState(std::move(state));
             }},
            {"58_court_revived",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M49: the revive announcement. The guards are felled and the
                 // King's clock is wound to one turn short, so the captured line
                 // is the real one the shared rule produces.
                 battle::Battle b = battle::buildBattle(c.party, kingTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                            MusicTrack::KingBattle);
                 state->captureCourtRevival();
                 s.pushState(std::move(state));
             }},
            {"56_battle_target_affinity",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M48: the target-info panel for a foe with BOTH affinities —
                 // the two chips on the vitals row, checked against the panel's
                 // 60px budget.
                 dungeon::EnemyTeam team;
                 team.bossId = "frost_monarch";
                 team.statScalePct = 100;
                 battle::Battle b = battle::buildBattle(c.party, team, c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureEnterTargeting();
                 s.pushState(std::move(state));
             }},
            {"55_bestiary_affinity",
             [](StateStack& s, AppContext& c) {
                 // M48: a known foe carrying BOTH a weakness and an immunity, so
                 // the affinity block is overflow-checked alongside the passives
                 // and flavor it shares the panel with.
                 c.party.encountered.push_back("frost_monarch");
                 auto state = std::make_unique<BestiaryState>(s, c);
                 state->captureSelect("frost_monarch");
                 s.pushState(std::move(state));
             }},
            {"52_dungeon_quit",
             [](StateStack& s, AppContext& c) {
                 // M47: the same question from inside a run — the taller pause
                 // panel with its new Quit row, and the dungeon-honest body.
                 s.pushState(std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 8, c.content, "crystal_mine")));
                 s.pushState(std::make_unique<DungeonMenuState>(s, c));
                 pushQuitPrompt(s, c, quit::kDungeonBody);
             }},
            {"62_battle_log",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M52: the scrollable battle log over a live battle, filled with
                 // worst-case long lines and scrolled up so both the more-above
                 // and more-below carets render — the overflow check for the
                 // wrapped list.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot));
                 BattleLog log;
                 for (int i = 0; i < 20; ++i) {
                     log.push("WWWWWWWWWWWW uses Sovereign's Cataclysm on The Hollow King. "
                              "HP +240. MP +60. ATK-/DEF- lifted. (" + std::to_string(i + 1) +
                              ")");
                 }
                 auto overlay = std::make_unique<BattleLogState>(s, c, log);
                 overlay->captureScrollUp(4);
                 s.pushState(std::move(overlay));
             }},
            {"63_equip_diff",
             [](StateStack& s, AppContext& c) {
                 // M52: the equip-item detail panel — the slot's current item plus
                 // the stat diff for the highlighted candidate. Member 0 wears a
                 // weapon and carries two candidate weapons, then the item list is
                 // opened on a real candidate (row 1).
                 c.party.currentTown = 7;
                 c.party.members[0].weapon = "iron_sword";
                 c.party.inventory.add("war_hammer", 1);
                 c.party.inventory.add("steel_sword", 2);
                 refreshCharacter(c.party.members[0], c.content);
                 auto state = std::make_unique<EquipShopState>(s, c);
                 state->captureEnterEquipItem(0, content::EquipSlot::Weapon);
                 s.pushState(std::move(state));
             }},
            {"115_equip_heirloom_text",
             [](StateStack& s, AppContext& c) {
                 // M98: the equip-item band for an HEIRLOOM candidate shows the
                 // piece's effect text where the stat diff sits (heirlooms have
                 // no stats to diff — the old zero row said nothing). Staged on
                 // the longest shipped composition (Hearthstone Chip) so the
                 // two-line "equipshop.heirloom" wrap is refereed at maximum
                 // authored length.
                 c.party.members[0].equippedHeirloom = "heirloom_lastlight";
                 c.party.inventory.add("heirloom_hearthstone", 1);
                 refreshCharacter(c.party.members[0], c.content);
                 auto state = std::make_unique<EquipShopState>(s, c);
                 state->captureEnterEquipItem(0, content::EquipSlot::Heirloom);
                 s.pushState(std::move(state));
             }},
            {"127_blackjack_cards",
             [](StateStack& s, AppContext& c) {
                 // Owner request 2026-08-29: the card table — dealer's row
                 // with the hole card down, the player's row, values and the
                 // hit/stand hints (seed picked for a live opening hand).
                 s.pushState(std::make_unique<BlackjackEventState>(
                     s, c, 25, /*seed=*/777, /*room=*/3, /*ev=*/nullptr));
             }},
            {"126_reels_spin",
             [](StateStack& s, AppContext& c) {
                 // Owner request 2026-08-29: the live spin, mid-animation —
                 // one cell landed, the wheel flicking through the rest, the
                 // Skip hint below (a fixed spin clock keeps it exact).
                 auto state = std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 8, c.content, "crystal_mine"));
                 state->captureShowReelsSpinning();
                 s.pushState(std::move(state));
             }},
            {"125_equip_slot_info",
             [](StateStack& s, AppContext& c) {
                 // Owner request 2026-08-28: the slot list's new info band,
                 // hovered on the worn heirloom — whose effect text is the
                 // longest thing the two-line "equipshop.slotinfo" wrap
                 // carries.
                 c.party.members[0].weapon = "iron_sword";
                 c.party.members[0].equippedHeirloom = "heirloom_lastlight";
                 refreshCharacter(c.party.members[0], c.content);
                 auto state = std::make_unique<EquipShopState>(s, c, /*partyMode=*/true);
                 state->captureEnterEquipSlot(0, 3);
                 s.pushState(std::move(state));
             }},
            {"116_stranger_joke",
             [](StateStack& s, AppContext& c) {
                 // M100: the optionless joke flow on the raised stage — the
                 // six-line panel budget and THE STRANGER "P" caption in one
                 // frame (joke_1 carries the owner's own line).
                 s.pushState(
                     std::make_unique<CutsceneState>(s, c, "joke_1", /*replay=*/true));
             }},
            {"64_settings_audio",
             [](StateStack& s, AppContext& c) {
                 // M52: the Audio submenu, showing the new Ambience Volume row
                 // between SFX and Background Audio.
                 auto st = std::make_unique<SettingsState>(s, c);
                 st->captureShowAudio();
                 s.pushState(std::move(st));
             }},
#ifdef CRYSTAL_DEBUG_OVERLAY
            {"65_debug_menu",
             [](StateStack& s, AppContext& c) {
                 // M53: the development-only debug console with its longest labels
                 // and widest values (max gold, the dungeon-only instant-clear
                 // row present). Only compiled when the debug overlay is enabled,
                 // which the capture (debug) build is.
                 c.party.gold = 9999999;
                 c.party.legendaryTokens = 99;
                 s.pushState(std::make_unique<DebugMenuState>(s, c, /*inDungeon=*/true));
             }},
#endif
            {"66_armory_ghost",
             [](StateStack& s, AppContext& c) {
                 // M55: the Ruined Keep's Armory Ghost, faced so its footer
                 // trade-off is checked in situ. Since the 2026-08-17 leveling
                 // the rite rolls at ~8% per floor, so the seed loop sweeps
                 // until one holds a stand-in-front-able marker (a 200-seed
                 // sweep misses with probability ~0.92^200 — never in practice).
                 for (std::uint64_t seed = 1; seed < 200; ++seed) {
                     dungeon::Dungeon d =
                         dungeon::generate(seed, 20, c.content, "ruined_keep", 7);
                     auto state = std::make_unique<DungeonState>(s, c, std::move(d));
                     if (state->captureFaceEvent(dungeon::RoomEventKind::ArmoryGhost)) {
                         s.pushState(std::move(state));
                         return;
                     }
                 }
             }},
            {"67_miners_cache",
             [](StateStack& s, AppContext& c) {
                 // M55: the Crystal Mine's Miner's Cache (leveled 2026-08-17;
                 // the sweep finds a seed that rolled it).
                 for (std::uint64_t seed = 1; seed < 200; ++seed) {
                     dungeon::Dungeon d =
                         dungeon::generate(seed, 20, c.content, "crystal_mine", 7);
                     auto state = std::make_unique<DungeonState>(s, c, std::move(d));
                     if (state->captureFaceEvent(dungeon::RoomEventKind::MinersCache)) {
                         s.pushState(std::move(state));
                         return;
                     }
                 }
             }},
            {"68_elder_root",
             [](StateStack& s, AppContext& c) {
                 // M55: the Hollow Forest's Elder Root (leveled 2026-08-17; the
                 // sweep finds a seed that rolled it). Gold is set high so the
                 // affordable "pay for XP" prompt shows (not the refusal).
                 c.party.gold = 99999;
                 for (std::uint64_t seed = 1; seed < 200; ++seed) {
                     dungeon::Dungeon d =
                         dungeon::generate(seed, 20, c.content, "hollow_forest", 7);
                     auto state = std::make_unique<DungeonState>(s, c, std::move(d));
                     if (state->captureFaceEvent(dungeon::RoomEventKind::ElderRoot)) {
                         s.pushState(std::move(state));
                         return;
                     }
                 }
             }},
            // M56: the four per-theme battle backdrops behind a live battle, one
            // per stage, so the subdued dressing is checked against the combatants
            // and the grounding.
            {"69_backdrop_keep",
             [&battleSlot](StateStack& s, AppContext& c) {
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                           MusicTrack::None, nullptr, false,
                                                           render::BackdropStage::Keep));
             }},
            {"70_backdrop_mine",
             [&battleSlot](StateStack& s, AppContext& c) {
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                           MusicTrack::None, nullptr, false,
                                                           render::BackdropStage::Mine));
             }},
            {"71_backdrop_forest",
             [&battleSlot](StateStack& s, AppContext& c) {
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                           MusicTrack::None, nullptr, false,
                                                           render::BackdropStage::Forest));
             }},
            {"72_backdrop_castle",
             [&battleSlot](StateStack& s, AppContext& c) {
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                           MusicTrack::None, nullptr, false,
                                                           render::BackdropStage::Castle));
             }},
            {"73_backdrop_mine_hc",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // High contrast: the Mine backdrop must drop its crystal accent but
                 // keep the silhouettes. (The loop resets high contrast after.)
                 ui::style::setHighContrast(true);
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                           MusicTrack::None, nullptr, false,
                                                           render::BackdropStage::Mine));
             }},
            {"74_boss_intro_build",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M56: the Crystal Shatter mid-Build (growing cracks + the boss
                 // name/telegraph), frozen. The King has the longest telegraph.
                 battle::Battle b = battle::buildBattle(c.party, kingTeam(c.content), c.content);
                 auto st = std::make_unique<BossIntroState>(s, c, std::move(b), &battleSlot,
                                                            MusicTrack::None, nullptr, false,
                                                            render::BackdropStage::Castle, 42u);
                 st->captureSetTime(BossIntroTimeline::kHold + BossIntroTimeline::kBuild * 0.65f);
                 s.pushState(std::move(st));
             }},
            {"75_boss_intro_peak",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M56: the Peak beat (dim pulse + shake) with the longest telegraph.
                 battle::Battle b = battle::buildBattle(c.party, kingTeam(c.content), c.content);
                 auto st = std::make_unique<BossIntroState>(s, c, std::move(b), &battleSlot,
                                                            MusicTrack::None, nullptr, false,
                                                            render::BackdropStage::Castle, 42u);
                 st->captureSetTime(BossIntroTimeline::kBuildEnd + BossIntroTimeline::kPeak * 0.5f);
                 s.pushState(std::move(st));
             }},
            {"76_goose_town",
             [](StateStack& s, AppContext& c) {
                 // M61: the Goose Town hub with a felled-Duck record — the
                 // fullest layout for the overflow check.
                 c.party.gooseTownUnlocked = true;
                 c.party.castleRecords.duckBestTurns = 27;
                 s.pushState(std::make_unique<GooseTownState>(s, c));
             }},
            {"77_duck_battle",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M61: the Deadly Duck fight — the biggest HP number the battle
                 // panel will ever show, plus his telegraph and passive chips.
                 battle::Battle b = battle::buildBattle(c.party, duckTeam(c.content), c.content);
                 s.pushState(std::make_unique<BattleState>(s, c, std::move(b), &battleSlot,
                                                           MusicTrack::None, nullptr, false,
                                                           render::BackdropStage::Castle));
             }},
            {"82_curio_collection",
             [](StateStack& s, AppContext& c) {
                 // M66: the Maps screen at its fullest — a revealed treasure
                 // line + seven owned curios in the collection grid.
                 c.party.treasure.active = true;
                 c.party.treasure.town = 6;
                 c.party.treasure.bossId = bossRushOrder(c.content).front();
                 for (int i = 0; i < 7; ++i) {
                     c.party.ownedCurios.push_back(kCurios[i].id);
                 }
                 c.party.treasureScrollsAwarded = {"treasure_scroll_meteor"};
                 s.pushState(std::make_unique<MapsState>(s, c));
             }},
            {"80_puzzle_map",
             [](StateStack& s, AppContext& c) {
                 // M65: the half-solved puzzle map (two quadrants + status).
                 // Self-contained: scene 82 runs earlier on the same party,
                 // so its reveal/curios are cleared here. M83: the guild's IOU
                 // line rides the status (the longest combined form).
                 c.party.mapPieces = 2;
                 c.party.mapPiecesOwed = 2;
                 c.party.treasure = TreasureReveal{};
                 c.party.ownedCurios.clear();
                 c.party.treasureScrollsAwarded.clear();
                 s.pushState(std::make_unique<MapsState>(s, c));
             }},
            {"81_treasure_dig",
             [](StateStack& s, AppContext& c) {
                 // M65: the dug-up treasure overlay at its fullest — the
                 // scroll's learn-it-now text plus the member picker.
                 auto st = std::make_unique<TreasureFightState>(s, c);
                 st->captureResult();
                 s.pushState(std::move(st));
             }},
            {"79_party_panel",
             [](StateStack& s, AppContext& c) {
                 // M64/M67/M72: the party panel at its TRUE worst case — the
                 // owner's Lv.99 clip report: level cap, all three milestones
                 // chosen (Martyr carries the longest description), an
                 // equipped passive with its line, a scroll-learned skill
                 // mark, and a teaching scroll for the footer (member 0 is
                 // the cleric — class ids sort first).
                 if (!c.party.members.empty()) {
                     c.party.members[0].level = kMaxLevel;
                     c.party.members[0].extraSkills.push_back("fireball");
                     c.party.members[0].milestone10 = "cleric_10_a";
                     c.party.members[0].milestone20 = "cleric_20_a";
                     c.party.members[0].milestone30 = "cleric_30_a";
                     c.party.members[0].equippedPassive = "clarity";
                     c.party.members[0].ownedPassives = {"clarity"};
                     refreshCharacter(c.party.members[0], c.content);
                 }
                 c.party.inventory.add("scroll_whirlwind", 1);
                 s.pushState(std::make_unique<PartyState>(s, c));
             }},
            {"78_milestone_choice",
             [](StateStack& s, AppContext& c) {
                 // M63: the level-milestone choice modal over the town. The
                 // shown tier is FORCED via captureSelect; the member's choice
                 // is pre-set so the town's own auto-prompt stays quiet.
                 if (!c.party.members.empty()) {
                     c.party.members[0].level = 10;
                     c.party.members[0].milestone10 = "chosen";  // non-empty = not pending
                 }
                 s.pushState(std::make_unique<TownState>(s, c));
                 auto st = std::make_unique<MilestoneChoiceState>(s, c);
                 st->captureSelect(0, 10);
                 s.pushState(std::move(st));
             }},
            {"83_battle_spoils",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M68: the victory results panel at its fullest — four leveled
                 // members (12-char names), multi-skill learn lines, max
                 // XP/gold widths — over a settled battlefield.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureShowSpoils();
                 s.pushState(std::move(state));
             }},
            {"85_battle_details",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // The unit Details overlay at the fullest layout the panel's
                 // line budget admits (guard line + four status chips — see
                 // captureOpenDetails for the known Passive-line gap), so the
                 // wrapped status legend (TRF/STN joined it after M75) is
                 // overflow-checked.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 BattleState* raw = state.get();
                 s.pushState(std::move(state));
                 raw->captureOpenDetails();  // queues the overlay push after the battle's
             }},
            {"98_story_long",
             [](StateStack& s, AppContext& c) {
                 // M87: the storyteller panel with a pseudo-translated LONGEST
                 // beat — the panel must cap inside the safe area and scroll
                 // (more-below arrow), never grow off the 426x240 screen.
                 // (No TownState underneath: an earlier guild scene leaves a
                 // pending town-milestone offer that would modal over this.)
                 if (const content::StoryBeat* beat =
                         c.content.findStoryBeat(kDragonJesterBeat)) {
                     // Doubled: even the safe-area cap (~14 lines) overflows,
                     // so the more-below arrow and scroll hint are in frame.
                     s.pushState(std::make_unique<StoryDialogState>(
                         s, c, beat->speaker, beat->title,
                         pseudoLocalize(beat->body + "\n\n" + beat->body)));
                 }
             }},
            {"99_details_long",
             [](StateStack& s, AppContext& c) {
                 // M87: the canonical reading overlay on a pseudo-translated
                 // scoring text, SCROLLED to the bottom clamp — the more-above
                 // arrow shows and the tail is the Latin pangram, pinning the
                 // whole new glyph set as actually rendered.
                 auto st = std::make_unique<DetailsOverlayState>(
                     s, c, "How Scoring Works",
                     pseudoLocalize(scoreDetailsText()) + "\n\n" + kLatinPangram);
                 st->captureScroll(999);
                 s.pushState(std::move(st));
             }},
            {"100_bestiary_read",
             [](StateStack& s, AppContext& c) {
                 // M87: the King's flavor at the BODY font (the shrink-to-fit
                 // policy is gone), stretched past the panel and put in read
                 // focus — focus brackets + scrolling viewport + swapped
                 // footer hints.
                 c.party.encountered.push_back(kKingBossId);
                 auto state = std::make_unique<BestiaryState>(s, c);
                 state->captureSelect(kKingBossId);
                 state->captureStretchFlavor();
                 s.pushState(std::move(state));
             }},
            {"101_party_details",
             [](StateStack& s, AppContext& c) {
                 // M87: the full member sheet behind Details — the maxed
                 // cleric of scene 79 (three milestones, passive, scroll
                 // skill), now with every skill's DESCRIPTION, long enough
                 // to scroll.
                 if (!c.party.members.empty()) {
                     c.party.members[0].level = kMaxLevel;
                     c.party.members[0].extraSkills.push_back("fireball");
                     c.party.members[0].milestone10 = "cleric_10_a";
                     c.party.members[0].milestone20 = "cleric_20_a";
                     c.party.members[0].milestone30 = "cleric_30_a";
                     c.party.members[0].equippedPassive = "clarity";
                     c.party.members[0].ownedPassives = {"clarity"};
                     refreshCharacter(c.party.members[0], c.content);
                 }
                 auto state = std::make_unique<PartyState>(s, c);
                 state->captureSelect(0);
                 PartyState* raw = state.get();
                 s.pushState(std::move(state));
                 raw->captureOpenDetails();  // queued AFTER the panel's own push
             }},
            {"102_battle_skill_preview",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M87: the 2-line battle preview on the capture-stretch
                 // skill — the stepped more-arrow marks the intentional
                 // truncation and the scene passes the zero-overflow check
                 // (policy B never counts as a defect).
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 state->captureEnterSkillMenu({"zz_capture_stretch"});
                 s.pushState(std::move(state));
             }},
            {"103_battle_skill_details",
             [&battleSlot](StateStack& s, AppContext& c) {
                 // M87: Details during skill selection — the full skill sheet
                 // (MP cost + complete description) in the scrolling overlay.
                 battle::Battle b =
                     battle::buildBattle(c.party, makeFiveEnemyTeam(c.content), c.content);
                 auto state = std::make_unique<BattleState>(s, c, std::move(b), &battleSlot);
                 BattleState* raw = state.get();
                 s.pushState(std::move(state));
                 raw->captureOpenSkillDetails({"zz_capture_stretch"});
             }},
            {"104_event_flavor_long",
             [](StateStack& s, AppContext& c) {
                 // M87: the Peddler's panel with a pseudo-translated body —
                 // the flavor scrolls while the title, the gold trade-off
                 // line, and the step-away hint hold their fixed places.
                 const content::EventFlavorDef* flavor = c.content.findEventFlavor(
                     dungeon::eventFlavorId(dungeon::RoomEventKind::DuckPeddler));
                 if (flavor == nullptr) {
                     return;
                 }
                 for (std::uint64_t seed = 1; seed < 4000; ++seed) {
                     dungeon::Dungeon d =
                         dungeon::generate(seed, 8, c.content, "crystal_mine", 3);
                     bool holdsPeddler = false;
                     for (const dungeon::Room& r : d.rooms) {
                         holdsPeddler = holdsPeddler ||
                                        r.event.kind == dungeon::RoomEventKind::DuckPeddler;
                     }
                     if (!holdsPeddler) {
                         continue;
                     }
                     auto state = std::make_unique<DungeonState>(s, c, std::move(d));
                     // A doubled body simulates the truly verbose translation:
                     // well past the 4 visible lines, so the scroll indicator
                     // and the fixed trade-off line are both in frame.
                     if (state->captureOpenEventPanel(
                             dungeon::RoomEventKind::DuckPeddler,
                             pseudoLocalize(flavor->body + " " + flavor->body))) {
                         s.pushState(std::move(state));
                         return;
                     }
                 }
             }},
            {"105_event_outcome_long",
             [](StateStack& s, AppContext& c) {
                 // M87: a pseudo-translated outcome — the body scrolls inside
                 // the fixed box instead of truncating at three lines.
                 auto state = std::make_unique<DungeonState>(
                     s, c, dungeon::generate(424242, 8, c.content, "crystal_mine"));
                 state->captureShowOutcome(
                     "The Chest",
                     pseudoLocalize("The trap bites - the party is wounded! Found 1240 gold "
                                    "+ Hi-Potion, and the guild's appraisal committee "
                                    "appends its full report in every official language."));
                 s.pushState(std::move(state));
             }},
            {"84_celebration",
             [](StateStack& s, AppContext& c) {
                 // M71: the victory celebration at its fullest — max score
                 // width, the MVP (12-char name) on the pedestal, one fallen
                 // member lying down. Runs LAST: the KO'd member leaks into no
                 // later scene.
                 if (c.party.members.size() > 2) {
                     c.party.members[2].hp = 0;
                 }
                 s.pushState(std::make_unique<CelebrationState>(s, c, "Score: 999999",
                                                                /*mvpIndex=*/0));
             }},
        };

        for (const Scenario& scenario : scenarios) {
            ++scenes;
            {
                StateStack stack;
                scenario.setup(stack, ctx);
                stack.applyPending();
                for (int i = 0; i < 30; ++i) {  // settle animations/timers
                    stack.update(1.0f / 60.0f);
                    stack.applyPending();
                }
                const long before = ui::overflowEvents();
                screen.beginDraw(BLACK);
                stack.render();
                screen.endDraw();
                const long overflowed = ui::overflowEvents() - before;
                const fs::path file = out / (std::string(scenario.name) + ".png");
                if (!screen.exportImage(file.string().c_str())) {
                    std::fprintf(stderr, "capture: FAILED to export %s\n",
                                 file.string().c_str());
                    ++failures;
                } else if (overflowed > 0) {
                    std::fprintf(stderr,
                                 "capture: %s has %ld text-overflow event(s) — see the "
                                 "[ui-overflow] log lines above\n",
                                 scenario.name, overflowed);
                    ++failures;
                } else {
                    std::printf("capture: %s ok\n", scenario.name);
                }
            }
            ui::style::setHighContrast(false);  // scenario 22 opts in per scene
        }
    }
    CloseWindow();
    fs::remove_all(scratch, ec);

    std::printf("capture: %d/%d scenes clean -> %s\n", scenes - failures, scenes,
                out.string().c_str());
    return failures == 0 ? 0 : 1;
}

}  // namespace cd::capture

#endif  // CRYSTAL_CAPTURE
