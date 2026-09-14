#pragma once

// M116 — the End-game Summary: the pure side of the tabbed statistics
// screen. Unlock (the finale's keepsake choice is recorded), the once-only
// auto-show flag, the six pages' rows built from the M109 lifetime ledger
// (display-only; nothing here writes), and the two formatters. The state
// (states/EndgameSummaryState) only lays these rows out.

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "game/Cutscenes.hpp"
#include "game/Lifetime.hpp"
#include "game/Party.hpp"
#include "game/Scrolls.hpp"
#include "game/WorldLadder.hpp"

namespace cd {

enum class SummaryPage { Overview, Heroes, Combat, World, Bestiary, Patrols };
inline constexpr int kSummaryPageCount = 6;

inline const char* summaryPageName(SummaryPage p) {
    switch (p) {
        case SummaryPage::Overview: return "Overview";
        case SummaryPage::Heroes: return "Heroes";
        case SummaryPage::Combat: return "Combat";
        case SummaryPage::World: return "World";
        case SummaryPage::Bestiary: return "Bestiary";
        case SummaryPage::Patrols: return "Patrols";
    }
    return "Overview";
}

inline SummaryPage summaryPageAt(int index) {
    const int n = ((index % kSummaryPageCount) + kSummaryPageCount) % kSummaryPageCount;
    return static_cast<SummaryPage>(n);
}

// One list row: a label with a right-aligned value, or a section header
// (disabled row, no value — the §6 convention).
struct SummaryRow {
    std::string label;
    std::string value;
    bool header = false;
};

// 1234567 -> "1,234,567". Counts are never negative; a negative one is
// shown as-is with its sign.
inline std::string formatCount(LifetimeCount v) {
    const bool neg = v < 0;
    std::string digits = std::to_string(neg ? -v : v);
    std::string out;
    int n = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (n > 0 && n % 3 == 0) {
            out.push_back(',');
        }
        out.push_back(*it);
        ++n;
    }
    std::reverse(out.begin(), out.end());
    return neg ? "-" + out : out;
}

// 3725 s -> "1h 02m".
inline std::string formatPlayTime(LifetimeCount seconds) {
    const LifetimeCount s = std::max<LifetimeCount>(0, seconds);
    const LifetimeCount h = s / 3600;
    const LifetimeCount m = (s % 3600) / 60;
    const std::string mm = (m < 10 ? "0" : "") + std::to_string(m);
    return std::to_string(h) + "h " + mm + "m";
}

// The summary exists once the finale's keepsake choice is recorded — the
// story's end, not the King's fall.
inline bool summaryUnlocked(const Party& party) {
    return !game::cutsceneChoiceFor(party, "finale").empty();
}

// Show itself once, automatically; afterwards P offers it.
inline bool shouldAutoShowSummary(const Party& party) {
    return summaryUnlocked(party) && !party.summaryShown;
}

namespace summary_detail {

inline void row(std::vector<SummaryRow>& out, const char* label, LifetimeCount v) {
    out.push_back({label, formatCount(v), false});
}
inline void header(std::vector<SummaryRow>& out, const std::string& title) {
    out.push_back({title, "", true});
}
inline std::string memberName(const Party& party, int slot) {
    if (slot >= 0 && slot < static_cast<int>(party.members.size())) {
        return party.members[static_cast<std::size_t>(slot)].name;
    }
    return "";
}

}  // namespace summary_detail

inline std::vector<SummaryRow> summaryRows(SummaryPage page, const Party& party,
                                           const content::ContentDatabase& db) {
    using namespace summary_detail;
    const LifetimeStats& L = party.lifetime;
    std::vector<SummaryRow> out;
    switch (page) {
        case SummaryPage::Overview: {
            if (L.migrated) {
                header(out, "Lifetime tracking began with this version");
            }
            out.push_back({"Play time", formatPlayTime(L.explore.playSeconds), false});
            out.push_back({"Dungeon clears / attempts",
                           formatCount(L.explore.runsCompleted) + " / " +
                               formatCount(L.explore.runsAttempted),
                           false});
            row(out, "Battles won", L.combat.battlesWon);
            row(out, "Enemies defeated", L.combat.enemiesKo);
            row(out, "Boss victories", L.combat.bossesDefeated);
            row(out, "Total damage dealt", L.combat.damageDealt);
            {
                const std::string who = memberName(party, L.combat.highestHitMember);
                out.push_back({"Biggest hit", formatCount(L.combat.highestHit) +
                                                  (who.empty() ? "" : " (" + who + ")"),
                               false});
            }
            row(out, "Gold earned", L.economy.goldEarned);
            row(out, "Gold spent", L.economy.goldSpent);
            row(out, "Patrols met", L.patrols.total);
            row(out, "King defeats", L.explore.kingDefeats);
            row(out, "Duck defeats", L.explore.duckDefeats);
            row(out, "Dragon defeats", L.explore.dragonDefeats);
            row(out, "Guild Master defeats", L.explore.guildMasterDefeats);
            row(out, "Eternal best floor", party.eternalBestFloors);
            row(out, "Floors cleared", L.explore.floorsCleared);
            row(out, "Tiles walked", L.explore.tilesWalked);
            break;
        }
        case SummaryPage::Heroes: {
            for (std::size_t i = 0; i < party.members.size() && i < L.members.size(); ++i) {
                const Character& c = party.members[i];
                const MemberLifetime& m = L.members[i];
                const content::ClassDef* cls = db.findClass(c.classId);
                header(out, c.name + "  Lv." + std::to_string(c.level) + "  " +
                                (cls != nullptr ? cls->name : c.classId));
                row(out, "  Damage dealt", m.damageDealt);
                row(out, "  Biggest hit", m.biggestHit);
                row(out, "  Finishing blows", m.finishingBlows);
                row(out, "  Times knocked out", m.timesKo);
                row(out, "  Damage taken", m.damageTaken);
                row(out, "  Healing done", m.healingDone);
                row(out, "  Healing received", m.healingReceived);
                row(out, "  Revives performed", m.revivesPerformed);
                row(out, "  Skills cast", m.skillsCast);
                row(out, "  Statuses inflicted", m.statusesInflicted);
                row(out, "  Scrolls learned", m.scrollsLearned);
                row(out, "  Skills known", static_cast<LifetimeCount>(allKnownSkills(c, db).size()));
            }
            break;
        }
        case SummaryPage::Combat: {
            row(out, "Battles won", L.combat.battlesWon);
            row(out, "Battles lost", L.combat.battlesLost);
            row(out, "Escapes", L.combat.playerEscapes);
            row(out, "Battle turns", L.combat.battleTurns);
            row(out, "Enemies defeated", L.combat.enemiesKo);
            row(out, "Boss victories", L.combat.bossesDefeated);
            row(out, "Damage dealt", L.combat.damageDealt);
            row(out, "Damage taken", L.combat.damageTaken);
            row(out, "Healing", L.combat.healing);
            row(out, "Statuses applied", L.combat.statusesApplied);
            row(out, "Revives", L.combat.revives);
            row(out, "Summons called", L.combat.summonsUsed);
            row(out, "Guards", L.combat.guardUses);
            {
                const std::string who = memberName(party, L.combat.highestHitMember);
                out.push_back({"Biggest hit", formatCount(L.combat.highestHit) +
                                                  (who.empty() ? "" : " (" + who + ")"),
                               false});
            }
            break;
        }
        case SummaryPage::World: {
            for (int town = 1; town <= kTownCount; ++town) {
                const TownLifetime& t = L.towns[static_cast<std::size_t>(town - 1)];
                header(out, "Town " + std::to_string(town));
                row(out, "  Attempts", t.attempts);
                row(out, "  Clears", t.clears);
                row(out, "  Wipes", t.wipes);
                row(out, "  Retreats", t.retreats);
                row(out, "  Battles won", t.battlesWon);
                row(out, "  Bosses defeated", t.bossesDefeated);
                row(out, "  Patrols", t.patrols);
                row(out, "  Gold earned", t.goldEarned);
                row(out, "  Gold spent", t.goldSpent);
                row(out, "  Best score", t.bestScore);
            }
            header(out, "Economy");
            row(out, "  Gold earned", L.economy.goldEarned);
            row(out, "  Gold spent", L.economy.goldSpent);
            row(out, "  Gold lost to defeat", L.economy.goldLost);
            row(out, "  Largest purse", L.economy.largestPurse);
            row(out, "  Items bought", L.economy.itemsBought);
            row(out, "  Equipment bought", L.economy.equipmentBought);
            row(out, "  Gold spent at inns", L.economy.innGold);
            row(out, "  Passives bought", L.economy.passivesBought);
            row(out, "  Scrolls learned", L.economy.scrollsLearned);
            row(out, "  Level-ups", L.economy.levelUps);
            row(out, "  Treasure found", L.economy.treasureFound);
            row(out, "  Tokens earned", L.economy.tokensEarned);
            row(out, "  Tokens spent", L.economy.tokensSpent);
            row(out, "  Map treasures dug", L.economy.mapTreasures);
            row(out, "  Curios found", L.economy.curiosFound);
            header(out, "Exploration");
            row(out, "  Runs attempted", L.explore.runsAttempted);
            row(out, "  Runs completed", L.explore.runsCompleted);
            row(out, "  Floors cleared", L.explore.floorsCleared);
            row(out, "  Tiles walked", L.explore.tilesWalked);
            row(out, "  Chests opened", L.explore.chestsOpened);
            row(out, "  Events resolved", L.explore.eventsResolved);
            row(out, "  Eternal floors", L.explore.eternalFloors);
            break;
        }
        case SummaryPage::Bestiary: {
            // The BestiaryState order: enemies then bosses, each by name; a foe
            // the party has not met stays a "? ? ?" row with no count — nothing
            // is ever marked seen from here.
            std::vector<std::pair<std::string, std::string>> foes;  // (name, id)
            for (const auto& [id, def] : db.enemies()) {
                foes.emplace_back(def.name, id);
            }
            std::sort(foes.begin(), foes.end());
            std::vector<std::pair<std::string, std::string>> bosses;
            for (const auto& [id, def] : db.bosses()) {
                bosses.emplace_back(def.name, id);
            }
            std::sort(bosses.begin(), bosses.end());
            foes.insert(foes.end(), bosses.begin(), bosses.end());
            for (const auto& [name, id] : foes) {
                const bool known = std::find(party.encountered.begin(), party.encountered.end(),
                                             id) != party.encountered.end();
                if (!known) {
                    out.push_back({"? ? ?", "", false});
                    continue;
                }
                const auto it = L.defeats.find(id);
                out.push_back({name, "Defeated: " + formatCount(it != L.defeats.end() ? it->second : 0),
                               false});
            }
            break;
        }
        case SummaryPage::Patrols: {
            row(out, "Patrols met", L.patrols.total);
            row(out, "Ordinary patrols", L.patrols.normal);
            row(out, "Golden Geese met", L.patrols.geeseMet);
            row(out, "Golden Geese caught", L.patrols.geeseDefeated);
            row(out, "Golden Geese escaped", L.patrols.geeseEscaped);
            row(out, "Lore questions asked", L.patrols.loreAttempted);
            row(out, "Lore answered right", L.patrols.loreCorrect);
            row(out, "Lore answered wrong", L.patrols.loreWrong);
            row(out, "Struck the Jester", L.patrols.jesterPunish);
            row(out, "Swept the field", L.patrols.aoePunish);
            row(out, "Reward chests", L.patrols.rewardChests);
            row(out, "Empty chests", L.patrols.emptyChests);
            row(out, "Mimics revealed", L.patrols.mimicsRevealed);
            row(out, "Mimics defeated", L.patrols.mimicsDefeated);
            row(out, "Stranger scenes", L.patrols.strangerScenes);
            break;
        }
    }
    return out;
}

}  // namespace cd
