#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"
#include "game/Relics.hpp"

// M44 Royal Relics & the doubled King: the seeded replacement event and its
// reload-proof grant, the four relic effects (all data-driven), the Terrified /
// Stunned turn-control statuses enforced in shared code, and the sim evidence
// that the re-statted King needs the obtainable counterplay.

using namespace cd;
using namespace cd::battle;

namespace {

content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep));
    return db;
}

// The same maxed endgame party the M40 castle battery uses: L50, top legendary
// gear (never the King's own reward), best passives.
Party maxedParty(const content::ContentDatabase& db) {
    Party p;
    struct Loadout {
        const char* cls;
        const char* passive;
    };
    const Loadout loadouts[] = {
        {"knight", "counter_attack"},
        {"ranger", "keen_senses"},
        {"mage", "spell_ward"},
        {"cleric", "clarity"},
    };
    for (const Loadout& l : loadouts) {
        if (const content::ClassDef* c = db.findClass(l.cls)) {
            Character ch = createCharacter(*c, l.cls, kMaxLevel);
            ch.weapon = std::string(l.cls) == "mage" ? "voidpiercer_rod" : "worldbreaker_axe";
            ch.armor = "aegis_eternal";
            ch.accessory = "titanforged_heart";
            ch.equippedPassive = l.passive;
            refreshCharacter(ch, db);
            p.members.push_back(ch);
        }
    }
    return p;
}

Combatant partyMember(const std::string& name, int hp, int atk, int spd) {
    Combatant c;
    c.side = Side::Party;
    c.name = name;
    c.sourceId = "hero";
    c.stats = {hp, atk, 10, 10, spd};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 30;
    return c;
}

Combatant enemyUnit(const std::string& name, const std::string& sourceId, int hp) {
    Combatant c;
    c.side = Side::Enemy;
    c.name = name;
    c.sourceId = sourceId;
    c.stats = {hp, 30, 20, 20, 12};
    c.maxHp = hp;
    c.hp = hp;
    c.mp = c.maxMp = 30;
    return c;
}

Battle board() {
    Battle b;
    b.units.push_back(partyMember("Aldo", 200, 30, 20));  // 0
    b.units.push_back(partyMember("Bria", 200, 20, 15));  // 1
    b.units.push_back(enemyUnit("Ogre", "ogre", 900));    // 2
    b.threat.assign(b.units.size(), 0);
    b.rngSeed = 0x5EED4444ull;
    return b;
}

const content::ItemDef& relic(const content::ContentDatabase& db, const char* id) {
    const content::ItemDef* it = db.findItem(id);
    REQUIRE(it != nullptr);
    return *it;
}

// One scripted battle round-loop, mirroring simulateInPlace but letting the
// caller spend a party member's turn on an item first. It drives the REAL shared
// pieces (turnOrder / tickStatuses / choosePartyAction / chooseEnemyAction /
// applyChoice), so the battery cannot drift from how battles actually resolve.
using TurnHook = std::function<bool(Battle&, int actor, int round)>;

SimResult runScripted(Battle& b, const content::ContentDatabase& db, int maxRounds,
                      const TurnHook& hook) {
    int rounds = 0;
    while (b.outcome() == Outcome::Ongoing && rounds < maxRounds) {
        ++rounds;
        b.turnsTaken = rounds;
        b.beginRound();
        for (int actor : turnOrder(b)) {
            if (!b.units[static_cast<std::size_t>(actor)].alive()) {
                continue;
            }
            b.tickStatuses(actor);
            if (!b.units[static_cast<std::size_t>(actor)].alive()) {
                continue;
            }
            b.clearGuard(actor);
            const bool party = b.units[static_cast<std::size_t>(actor)].side == Side::Party;
            // A forced turn is never the player's to spend (M43/M44).
            const bool forced =
                forcedActionFor(b.units[static_cast<std::size_t>(actor)]) != ForcedAction::None;
            if (party && !forced && hook && hook(b, actor, rounds)) {
                // the hook spent this member's turn on an item
            } else if (party) {
                applyChoice(b, actor, choosePartyAction(b, actor, db), db);
            } else {
                applyChoice(b, actor, chooseEnemyAction(b, actor, db), db);
            }
            if (b.outcome() != Outcome::Ongoing) {
                break;
            }
        }
    }
    SimResult r;
    r.outcome = b.outcome();
    r.rounds = rounds;
    for (const Combatant& u : b.units) {
        if (u.side == Side::Party) {
            r.partyMaxHp += u.maxHp;
            r.partyHpRemaining += (u.hp > 0 ? u.hp : 0);
            if (u.alive()) {
                ++r.partyAlive;
            }
        }
    }
    return r;
}

int kingIndex(const Battle& b) {
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        if (b.units[i].sourceId == std::string(kKingBossId)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int weakestLivingAlly(const Battle& b) {
    int best = -1;
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const Combatant& u = b.units[i];
        if (u.side == Side::Party && u.alive() &&
            (best < 0 || u.hp * 100 / u.maxHp <
                             b.units[static_cast<std::size_t>(best)].hp * 100 /
                                 b.units[static_cast<std::size_t>(best)].maxHp)) {
            best = static_cast<int>(i);
        }
    }
    return best;
}

// The obtainable counterplay: Tax Sheets, then the Evil Goose, then Royal Snacks
// whenever someone is badly hurt. NEVER the 5 % Deadly Spoon (the acceptance bar
// is that the King falls without it).
struct Counterplay {
    int sheets = 0;  // relics are consumed on use; a player may hold several
    int geese = 0;   // (each reliquary grants one, and a used relic can drop again)
    int snacks = 0;
    int crowns = 0;  // the 15% relic
    int spoons = 0;  // the 5% relic — the acceptance bar excludes it
};

TurnHook counterplayHook(const content::ContentDatabase& db, Counterplay plan) {
    auto state = std::make_shared<Counterplay>(plan);
    return [&db, state](Battle& b, int actor, int /*round*/) {
        const int king = kingIndex(b);
        if (king < 0 || !b.units[static_cast<std::size_t>(king)].alive()) {
            return false;
        }
        // Heal first when someone is about to fall — a dead ally deals no damage.
        const int hurt = weakestLivingAlly(b);
        if (state->snacks > 0 && hurt >= 0 &&
            b.units[static_cast<std::size_t>(hurt)].hp * 5 <
                b.units[static_cast<std::size_t>(hurt)].maxHp * 3) {  // below 60%
            --state->snacks;
            b.useItem(actor, hurt, relic(db, "royal_snacks"));
            return true;
        }
        const Combatant& k = b.units[static_cast<std::size_t>(king)];
        // The Spoon and the Crown are opening moves: they last the whole fight.
        if (state->spoons > 0) {
            --state->spoons;
            b.useItem(actor, king, relic(db, "deadly_spoon"));
            return true;
        }
        if (state->crowns > 0 && !hasStatus(k, content::StatusType::AttackDown)) {
            --state->crowns;
            b.useItem(actor, king, relic(db, "dragon_crown"));
            return true;
        }
        // Then buy turns, but never waste one on a King that is already stopped.
        if (forcedActionFor(k) == ForcedAction::None) {
            if (state->sheets > 0) {
                --state->sheets;
                b.useItem(actor, king, relic(db, "tax_sheets"));
                return true;
            }
            if (state->geese > 0) {
                --state->geese;
                b.useItem(actor, king, relic(db, "evil_goose"));
                return true;
            }
        }
        return false;
    };
}

}  // namespace

// --- The event: when it replaces a rolled event -----------------------------

TEST_CASE("relics: the replacement chance follows the town/depth table", "[relics]") {
    // Never outside the band.
    for (int depth : {1, 2, 5, 20}) {
        CHECK(relicEventChancePct(1, depth) == 0);
    }
    for (int town = 1; town <= 7; ++town) {
        CHECK(relicEventChancePct(town, 1) == 0);
    }
    // The band itself.
    CHECK(relicEventChancePct(2, 2) == 3);
    for (int town = 3; town <= 6; ++town) {
        CHECK(relicEventChancePct(town, 2) == 5);
    }
    CHECK(relicEventChancePct(7, 2) == 7);
    // The depth bonus applies from depth 20 UP (owner decision), not only at 20.
    CHECK(relicEventChancePct(2, 19) == 3);
    CHECK(relicEventChancePct(2, 20) == 8);
    CHECK(relicEventChancePct(2, 40) == 8);
    CHECK(relicEventChancePct(7, 20) == 12);
}

TEST_CASE("relics: the event never spawns below town 2 / depth 2, and never twice",
          "[relics][generation]") {
    const content::ContentDatabase db = loadContent();
    const auto relicRooms = [](const dungeon::Dungeon& d) {
        int n = 0;
        for (const dungeon::Room& r : d.rooms) {
            if (r.event.kind == dungeon::RoomEventKind::RoyalRelic) {
                ++n;
            }
        }
        return n;
    };
    for (std::uint64_t seed = 1; seed <= 300; ++seed) {
        CHECK(relicRooms(dungeon::generate(seed, 8, db, "ruined_keep", 1)) == 0);  // town 1
        CHECK(relicRooms(dungeon::generate(seed, 1, db, "ruined_keep", 7)) == 0);  // depth 1
        CHECK(relicRooms(dungeon::generate(seed, 20, db, "ruined_keep", 7)) <= 1);
    }
}

TEST_CASE("relics: the event does appear at the top of the ladder, and rarely",
          "[relics][generation]") {
    const content::ContentDatabase db = loadContent();
    int withRelic = 0;
    const int samples = 600;
    for (std::uint64_t seed = 1; seed <= static_cast<std::uint64_t>(samples); ++seed) {
        const dungeon::Dungeon d = dungeon::generate(seed, 20, db, "ruined_keep", 7);
        for (const dungeon::Room& r : d.rooms) {
            if (r.event.kind == dungeon::RoomEventKind::RoyalRelic) {
                ++withRelic;
                break;
            }
        }
    }
    INFO("town 7 / depth 20: " << withRelic << " of " << samples << " dungeons hold a reliquary");
    CHECK(withRelic > 0);                 // 12% per event, 2-3 events: it must happen
    CHECK(withRelic < samples * 45 / 100);  // but it stays a rare find
}

TEST_CASE("relics: generation stays deterministic with the event in the stream",
          "[relics][generation]") {
    const content::ContentDatabase db = loadContent();
    for (std::uint64_t seed : {7ull, 99ull, 4242ull}) {
        const dungeon::Dungeon a = dungeon::generate(seed, 20, db, "ruined_keep", 7);
        const dungeon::Dungeon b = dungeon::generate(seed, 20, db, "ruined_keep", 7);
        REQUIRE(a.rooms.size() == b.rooms.size());
        for (std::size_t i = 0; i < a.rooms.size(); ++i) {
            CHECK(a.rooms[i].event.kind == b.rooms[i].event.kind);
        }
    }
}

// --- The grant: seeded, reload-proof, renormalized ---------------------------

TEST_CASE("relics: the grant is deterministic and reload-proof", "[relics]") {
    const std::array<bool, kRelicCount> none{};
    for (std::uint64_t seed : {1ull, 12345ull, 0xFFFFFFFFull}) {
        for (int room = 0; room < 8; ++room) {
            CHECK(relicPickIndex(seed, room, none) == relicPickIndex(seed, room, none));
        }
    }
    // Different rooms of the same dungeon can differ (the salt includes the room).
    bool anyDifferent = false;
    for (int room = 1; room < 20 && !anyDifferent; ++room) {
        anyDifferent = relicPickIndex(777, room, none) != relicPickIndex(777, 0, none);
    }
    CHECK(anyDifferent);
}

TEST_CASE("relics: owned relics are excluded and the rest renormalized", "[relics]") {
    // Owning the two 40 % relics leaves only the Crown and the Spoon.
    std::array<bool, kRelicCount> owned{};
    owned[0] = true;  // evil_goose
    owned[1] = true;  // tax_sheets
    for (std::uint64_t seed = 1; seed <= 400; ++seed) {
        const int pick = relicPickIndex(seed, 3, owned);
        CHECK((pick == 2 || pick == 3));
    }
    // Owning three leaves exactly one possible grant.
    std::array<bool, kRelicCount> three{};
    three[0] = three[1] = three[2] = true;
    for (std::uint64_t seed = 1; seed <= 100; ++seed) {
        CHECK(relicPickIndex(seed, 1, three) == 3);
    }
}

TEST_CASE("relics: owning all four still grants a duplicate from the base roll",
          "[relics]") {
    std::array<bool, kRelicCount> all{};
    all.fill(true);
    std::array<int, kRelicCount> hits{};
    for (std::uint64_t seed = 1; seed <= 2000; ++seed) {
        const int pick = relicPickIndex(seed, 0, all);
        REQUIRE(pick >= 0);
        REQUIRE(pick < kRelicCount);
        ++hits[static_cast<std::size_t>(pick)];
    }
    // The base 40/40/15/5 table, not an exclusion: every relic can repeat, and the
    // common ones dominate.
    for (int h : hits) {
        CHECK(h > 0);
    }
    CHECK(hits[0] > hits[2]);
    CHECK(hits[1] > hits[3]);
}

TEST_CASE("relics: an unowned roll respects the authored weights", "[relics]") {
    const std::array<bool, kRelicCount> none{};
    std::array<int, kRelicCount> hits{};
    for (std::uint64_t seed = 1; seed <= 4000; ++seed) {
        ++hits[static_cast<std::size_t>(relicPickIndex(seed, 0, none))];
    }
    INFO("goose=" << hits[0] << " sheets=" << hits[1] << " crown=" << hits[2]
                  << " spoon=" << hits[3]);
    CHECK(hits[0] > hits[2]);  // 40 beats 15
    CHECK(hits[2] > hits[3]);  // 15 beats 5
    CHECK(hits[3] > 0);        // the spoon is rare, not impossible
}

// --- The relics in battle ----------------------------------------------------

TEST_CASE("relics: they are aimed at the enemy, not the party", "[relics][battle]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    const std::vector<int> targets = itemTargets(b, Side::Party, relic(db, "evil_goose"));
    CHECK(targets == std::vector<int>{2});  // the living foe, never an ally
}

TEST_CASE("relics: the Evil Goose forces a Guard for exactly one turn",
          "[relics][battle][forced]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    b.useItem(0, 2, relic(db, "evil_goose"));
    REQUIRE(hasStatus(b.units[2], content::StatusType::Terrified));
    CHECK(forcedActionFor(b.units[2]) == ForcedAction::Guard);

    // The foe's turn: it can only guard, and the enemy AI agrees.
    b.tickStatuses(2);
    b.clearGuard(2);
    const EnemyChoice forced = chooseEnemyAction(b, 2, db);
    CHECK(forced.forced == ForcedAction::Guard);
    applyChoice(b, 2, forced, db);
    CHECK(b.units[2].guarding);
    const int partyHp = b.units[0].hp + b.units[1].hp;

    // Its NEXT turn is free again (exactly one turn was taken).
    b.tickStatuses(2);
    b.clearGuard(2);
    CHECK_FALSE(hasStatus(b.units[2], content::StatusType::Terrified));
    const EnemyChoice free = chooseEnemyAction(b, 2, db);
    CHECK(free.forced == ForcedAction::None);
    applyChoice(b, 2, free, db);
    CHECK(b.units[0].hp + b.units[1].hp < partyHp);  // it hit back this time
}

TEST_CASE("relics: the Tax Sheets skip exactly one turn", "[relics][battle][forced]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    b.useItem(0, 2, relic(db, "tax_sheets"));
    REQUIRE(hasStatus(b.units[2], content::StatusType::Stunned));

    b.tickStatuses(2);
    b.clearGuard(2);
    const EnemyChoice skipped = chooseEnemyAction(b, 2, db);
    CHECK(skipped.forced == ForcedAction::Skip);
    const int before = b.units[0].hp + b.units[1].hp;
    applyChoice(b, 2, skipped, db);
    CHECK(b.units[0].hp + b.units[1].hp == before);  // nothing happened at all
    CHECK_FALSE(b.units[2].guarding);                 // not even a guard

    b.tickStatuses(2);
    b.clearGuard(2);
    CHECK(chooseEnemyAction(b, 2, db).forced == ForcedAction::None);
}

TEST_CASE("relics: a forced turn resolves identically for a party member (sim = live)",
          "[relics][battle][forced]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    b.units[0].statuses.push_back({content::StatusType::Stunned, 0, 2});
    // The party-side decider is the Simulator's; it must obey the same rule.
    CHECK(forcedActionFor(b.units[0]) == ForcedAction::Skip);
    CHECK(choosePartyAction(b, 0, db).forced == ForcedAction::Skip);
    CHECK(chooseEnemyAction(b, 0, db).forced == ForcedAction::Skip);
}

TEST_CASE("relics: the Dragon Crown only ever troubles the King", "[relics][battle][king]") {
    const content::ContentDatabase db = loadContent();
    const content::ItemDef& crown = relic(db, "dragon_crown");

    // On an ordinary foe: nothing, and the caller is told to keep it.
    Battle plain = board();
    plain.useItem(0, 2, crown);
    CHECK_FALSE(hasStatus(plain.units[2], content::StatusType::AttackDown));
    CHECK_FALSE(itemAffects(plain, 2, crown));

    // On the King: both debuffs land, and it is spent.
    Battle royal = board();
    royal.units[2] = enemyUnit("The Hollow King", kKingBossId, 3000);
    royal.units[2].isBoss = true;
    CHECK(itemAffects(royal, 2, crown));
    royal.useItem(0, 2, crown);
    CHECK(hasStatus(royal.units[2], content::StatusType::AttackDown));
    CHECK(hasStatus(royal.units[2], content::StatusType::DefenseDown));
}

TEST_CASE("relics: the Deadly Spoon halves a foe's stats for the rest of the battle",
          "[relics][battle]") {
    const content::ContentDatabase db = loadContent();
    Battle b = board();
    const content::StatBlock before = b.units[2].stats;
    const int hpBefore = b.units[2].hp;
    b.useItem(0, 2, relic(db, "deadly_spoon"));
    CHECK(b.units[2].stats.attack == before.attack / 2);
    CHECK(b.units[2].stats.magic == before.magic / 2);
    CHECK(b.units[2].stats.defense == before.defense / 2);
    CHECK(b.units[2].stats.speed == before.speed / 2);
    CHECK(b.units[2].hp == hpBefore);      // it weakens, it does not wound
    CHECK(b.units[2].maxHp == before.maxHp);
}

TEST_CASE("relics: the King is NOT immune to the relics - they are the counterplay",
          "[relics][battle][king]") {
    const content::ContentDatabase db = loadContent();
    Battle b = buildBattle(maxedParty(db), kingTeam(db), db);
    const int king = kingIndex(b);
    REQUIRE(king >= 0);
    b.useItem(0, king, relic(db, "evil_goose"));
    CHECK(forcedActionFor(b.units[static_cast<std::size_t>(king)]) == ForcedAction::Guard);
    b.useItem(0, king, relic(db, "tax_sheets"));
    CHECK(forcedActionFor(b.units[static_cast<std::size_t>(king)]) == ForcedAction::Skip);
}

// --- The doubled King --------------------------------------------------------

TEST_CASE("relics: the King carries the M44 stats", "[relics][king][data]") {
    const content::ContentDatabase db = loadContent();
    const content::BossDef* king = db.findBoss(kKingBossId);
    REQUIRE(king != nullptr);
    CHECK(king->stats.maxHp == 750);
    CHECK(king->stats.attack == 36);
    CHECK(king->stats.magic == 44);
    CHECK(king->stats.defense == 36);
    CHECK(king->stats.speed == 26);
}

TEST_CASE("relics: the counterplay measurably changes the King fight",
          "[relics][king][balance]") {
    // History: this case asserted the M44 bar — the approved counterplay WINS and
    // an unaided party loses — and every King scale from M44 to M49 was tuned to
    // satisfy it. The owner superseded that on 2026-07-23: the castle now starts
    // above the dungeon ceiling (see kKingScalePct / castleFloorScalePct) and is
    // explicitly not tuned to what the scripted sim can beat.
    //
    // What survives is the part that is still true and still worth protecting:
    // the relics must MATTER. The sim's fixed strategy is a floor on player
    // skill, so it is used here to prove the counterplay moves the fight in the
    // right direction, not to certify that the fight is winnable. Winnability is
    // an owner manual-validation item (matrix row 125), with the full ladder
    // recorded in [king-report].
    const content::ContentDatabase db = loadContent();

    Battle bare = buildBattle(maxedParty(db), kingTeam(db), db);
    const SimResult unaided = runScripted(bare, db, 400, nullptr);
    INFO("unaided: outcome=" << static_cast<int>(unaided.outcome) << " rounds="
                             << unaided.rounds << " kingHp%=" << unaided.partyHpFraction());

    Counterplay plan;
    plan.sheets = 1;
    plan.geese = 1;
    plan.snacks = 20;
    Battle armed = buildBattle(maxedParty(db), kingTeam(db), db);
    const SimResult aided = runScripted(armed, db, 400, counterplayHook(db, plan));
    INFO("with counterplay: outcome=" << static_cast<int>(aided.outcome)
                                      << " rounds=" << aided.rounds
                                      << " alive=" << aided.partyAlive);

    // The King is never a walkover for an unaided party.
    CHECK_FALSE(unaided.outcome == Outcome::Victory);
    // The counterplay must move the fight in the party's favour. Pre-M54 both
    // fights were losses, so "matters" meant the armed party survived strictly
    // longer. The **M54 equipment rebalance (doubled endgame gear)** tipped it:
    // the modest 1+1+20 "plan" counterplay now WINS (15 rounds, 2 survivors)
    // where the unaided maxed party still loses (21 rounds) — a stronger proof
    // that relics matter (a loss became a win), reported to the owner as a
    // balance shift (winnability stays an owner manual item, matrix row 125).
    // The assertion now checks the durable intent: the counterplay either wins
    // outright or (if a future retune makes both lose again) lasts strictly
    // longer. If a relic ever stops mattering, this still fails. (Computed into a
    // bool because Catch2's CHECK cannot decompose a top-level ||.)
    const bool counterplayHelped =
        aided.outcome == Outcome::Victory || aided.rounds > unaided.rounds;
    CHECK(counterplayHelped);
}

// On-demand table for the milestone note and owner review:
//   crystal_tests.exe "[king-report]" -s
TEST_CASE("king report: what it takes to fell the doubled King", "[.][king-report]") {
    const content::ContentDatabase db = loadContent();
    const content::BossDef* def = db.findBoss(kKingBossId);
    REQUIRE(def != nullptr);
    std::cout << "The Hollow King: base hp=" << def->stats.maxHp << " atk=" << def->stats.attack
              << " mag=" << def->stats.magic << " def=" << def->stats.defense
              << " spd=" << def->stats.speed << " at scale " << kKingScalePct << "%\n";
    {
        Battle probe = buildBattle(maxedParty(db), kingTeam(db), db);
        const int k = kingIndex(probe);
        std::cout << "  -> effective hp=" << probe.units[static_cast<std::size_t>(k)].maxHp
                  << " atk=" << probe.units[static_cast<std::size_t>(k)].stats.attack
                  << " mag=" << probe.units[static_cast<std::size_t>(k)].stats.magic
                  << " def=" << probe.units[static_cast<std::size_t>(k)].stats.defense
                  << " spd=" << probe.units[static_cast<std::size_t>(k)].stats.speed << "\n";
    }
    struct Row {
        const char* label;
        Counterplay plan;
    };
    const Row rows[] = {
        {"nothing", {}},
        {"30 snacks only", {0, 0, 30, 0, 0}},
        {"1 sheets + 1 goose + 20 snacks", {1, 1, 20, 0, 0}},
        {"2 + 2 + 30 snacks", {2, 2, 30, 0, 0}},
        {"3 + 3 + 30 snacks", {3, 3, 30, 0, 0}},
        {"3 + 3 + 30 snacks + crown", {3, 3, 30, 1, 0}},
        {"3 + 3 + 30 snacks + crown + SPOON", {3, 3, 30, 1, 1}},
    };
    std::cout << "counterplay | outcome (1=win 2=loss) | rounds | party alive | party hp%\n";
    for (const Row& row : rows) {
        Battle b = buildBattle(maxedParty(db), kingTeam(db), db);
        const SimResult r = runScripted(b, db, 400, counterplayHook(db, row.plan));
        std::cout << row.label << " | " << static_cast<int>(r.outcome) << " | " << r.rounds
                  << " | " << r.partyAlive << " | " << r.partyHpFraction() << "\n";
    }

    // Scale sweep across the CASTLE-FLOOR regime (owner decision 2026-07-23: every
    // castle scale sits above the 570 % dungeon ceiling). Two counterplay columns:
    // the M44 "plan" bar, and the absolute maximum a player could ever bring
    // (3 of each 40 % relic + the Crown + the Spoon + 30 snacks) — so the owner
    // can see not just whether the intended answer works, but whether ANY answer
    // the game contains does.
    const int floorPct = castleFloorScalePct(db);
    std::cout << "\nscale sweep — dungeon ceiling is " << floorPct << "%\n";
    std::cout << "scale% | eff hp | nothing | plan (1+1+20) | MAX (3+3+30+crown+spoon)\n";
    for (int scale : {440, 460, 480, kKingScalePct, 520, 540, floorPct + 10, 700}) {
        dungeon::EnemyTeam team = kingTeam(db);
        team.statScalePct = scale;
        Battle bare = buildBattle(maxedParty(db), team, db);
        const int effHp = bare.units[static_cast<std::size_t>(kingIndex(bare))].maxHp;
        const SimResult none = runScripted(bare, db, 400, nullptr);

        Counterplay plan;
        plan.sheets = 1;
        plan.geese = 1;
        plan.snacks = 20;
        Battle armed = buildBattle(maxedParty(db), team, db);
        const SimResult r = runScripted(armed, db, 400, counterplayHook(db, plan));

        Counterplay best;
        best.sheets = 3;
        best.geese = 3;
        best.snacks = 30;
        best.crowns = 1;
        best.spoons = 1;
        Battle maxed = buildBattle(maxedParty(db), team, db);
        const SimResult m = runScripted(maxed, db, 400, counterplayHook(db, best));

        std::cout << scale << " | " << effHp << " | " << static_cast<int>(none.outcome) << "/"
                  << none.rounds << " | " << static_cast<int>(r.outcome) << "/" << r.rounds << "/"
                  << r.partyAlive << " | " << static_cast<int>(m.outcome) << "/" << m.rounds << "/"
                  << m.partyAlive << "\n";
    }
    SUCCEED();
}
