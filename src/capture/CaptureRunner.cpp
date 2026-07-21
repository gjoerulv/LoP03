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
#include "states/BattleState.hpp"
#include "states/BlackMarketState.hpp"
#include "states/DetailsOverlayState.hpp"
#include "states/DungeonResultState.hpp"
#include "states/DungeonState.hpp"
#include "states/EquipShopState.hpp"
#include "states/GuildState.hpp"
#include "states/HelpState.hpp"
#include "states/InnState.hpp"
#include "states/ItemShopState.hpp"
#include "states/MainMenuState.hpp"
#include "states/PartyCreationState.hpp"
#include "states/RemapState.hpp"
#include "states/ScoreDetailsText.hpp"
#include "states/ScoreboardState.hpp"
#include "states/SettingsState.hpp"
#include "states/SlotMenuState.hpp"
#include "states/StateStack.hpp"
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
    return run;
}

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
    InitWindow(config::kVirtualWidth, config::kVirtualHeight, "CrystalDungeons capture");
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

        AppContext ctx{resources,  content, saves, party,
                       scoreboard, audio,   fade,  input,
                       settings,   tutorial,
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
        }

        battle::BattleResult battleSlot;  // outlives the battle scenes

        const tutorial::Beat* longestBeat = &tutorial::kBeats[0];
        for (const tutorial::Beat& b : tutorial::kBeats) {
            if (std::string(b.body).size() > std::string(longestBeat->body).size()) {
                longestBeat = &b;
            }
        }

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
                 s.pushState(std::make_unique<SettingsState>(s, c));
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
            {"11_slot_menu_save",
             [](StateStack& s, AppContext& c) {
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
             [longestBeat](StateStack& s, AppContext& c) {
                 s.pushState(std::make_unique<TownState>(s, c));
                 s.pushState(std::make_unique<TutorialPromptState>(
                     s, c, longestBeat->title, longestBeat->body));
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
                 c.party.blackMarket = {true, 6, "dawnforged_blade", 6500, 16, 6};  // M34 NPC
                 s.pushState(std::make_unique<TownState>(s, c));
             }},
            {"26_guild_penalty",
             [](StateStack& s, AppContext& c) {
                 // M33: a Guild whose configured run does not raise the stakes,
                 // so the forewarning shows a penalty. Placed after the town
                 // scenes; the stakes mutation does not leak into earlier scenes.
                 c.party.currentTown = 1;
                 c.party.stakes = {1, 20, 1};  // prev (town 1, depth 20); 1 prior step -> forewarns -60% (M35: 30/step)
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
