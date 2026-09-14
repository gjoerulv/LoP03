// M112 - the Jester's Lore question: the content category, the eligibility
// gate (no spoilers below a tier or before the King), the seeded pool walk,
// the field placeholders and the one resolution rule.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "game/Party.hpp"
#include "game/SpecialEncounter.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static content::ContentDatabase database;
    static bool loaded = false;
    if (!loaded) {
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), database, rep));
        loaded = true;
    }
    return database;
}

Party makeParty() {
    Party party;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        party.members.push_back(createCharacter(*cls, id, 12));
    }
    return party;
}

bool parses(const std::string& text) {
    content::ContentDatabase mem;
    content::LoadReport rep;
    const content::Json root = content::Json::parse(text, nullptr, false);
    REQUIRE_FALSE(root.is_discarded());
    content::parseLoreQuestions(root, "t.json", mem, rep);
    return rep.ok();
}

}  // namespace

TEST_CASE("lore: the shipped pool is 28 questions, about four a tier, one post-King",
          "[lore][content][data]") {
    REQUIRE(db().loreQuestionCount() == 28);
    std::map<int, int> perTier;
    int postKing = 0;
    for (const auto& [id, q] : db().loreQuestions()) {
        INFO(id);
        CHECK(q.minTown >= 1);
        CHECK(q.minTown <= 7);
        CHECK_FALSE(q.question.empty());
        CHECK_FALSE(q.answer.empty());
        CHECK_FALSE(q.wrongAnswer.empty());
        CHECK_FALSE(q.mockLine.empty());
        CHECK(q.answer != q.wrongAnswer);
        // The field boxes and the two-line prompt bound the authored lengths.
        CHECK(q.question.size() <= 120);
        CHECK(q.answer.size() <= 26);
        CHECK(q.wrongAnswer.size() <= 26);
        CHECK(q.mockLine.size() <= 80);
        ++perTier[q.minTown];
        if (q.postKing) {
            ++postKing;
        }
    }
    for (int tier = 1; tier <= 7; ++tier) {
        INFO("tier " << tier);
        CHECK(perTier[tier] == 4);
    }
    CHECK(postKing == 1);
}

TEST_CASE("lore: the loader owns the shape", "[lore][content]") {
    const std::string head = R"({"version":1,"questions":[)";
    const std::string tail = R"(]})";
    CHECK(parses(head + R"({"id":"q","minTown":3,"question":"Q?","answer":"A","wrongAnswer":"B","mockLine":"M."})" + tail));
    CHECK(parses(head + R"({"id":"q","question":"Q?","answer":"A","wrongAnswer":"B","mockLine":"M."})" + tail));  // minTown defaults
    CHECK_FALSE(parses(head + R"({"id":"q","minTown":8,"question":"Q?","answer":"A","wrongAnswer":"B","mockLine":"M."})" + tail));
    CHECK_FALSE(parses(head + R"({"id":"q","minTown":0,"question":"Q?","answer":"A","wrongAnswer":"B","mockLine":"M."})" + tail));
    CHECK_FALSE(parses(head + R"({"id":"q","question":"Q?","answer":"A","wrongAnswer":"A","mockLine":"M."})" + tail));
    CHECK_FALSE(parses(head + R"({"id":"q","question":"","answer":"A","wrongAnswer":"B","mockLine":"M."})" + tail));
    CHECK_FALSE(parses(head + R"({"id":"q","question":"Q?","answer":"A","wrongAnswer":"B"})" + tail));  // mockLine required
    CHECK_FALSE(parses(head + R"({"id":"q","question":"Q?","answer":"A","wrongAnswer":"B","mockLine":"M."},{"id":"q","question":"Q2?","answer":"A","wrongAnswer":"B","mockLine":"M."})" + tail));  // duplicate id
}

TEST_CASE("lore: eligibility never leaks a later tier or the King", "[lore]") {
    for (int highest = 1; highest <= 7; ++highest) {
        for (const bool king : {false, true}) {
            const std::vector<std::string> ids = eligibleLoreIds(db(), highest, king);
            for (const std::string& id : ids) {
                const content::LoreQuestionDef* q = db().findLoreQuestion(id);
                REQUIRE(q != nullptr);
                INFO(id << " at highest " << highest << " king " << king);
                CHECK(q->minTown <= highest);
                CHECK((!q->postKing || king));
            }
            // Sorted, so the seeded walk has a stable base.
            CHECK(std::is_sorted(ids.begin(), ids.end()));
        }
    }
    CHECK(eligibleLoreIds(db(), 1, false).size() == 4);
    CHECK(eligibleLoreIds(db(), 7, false).size() == 27);
    CHECK(eligibleLoreIds(db(), 7, true).size() == 28);
}

TEST_CASE("lore: the seeded walk asks every eligible question before a repeat", "[lore]") {
    const std::vector<std::string> pool = eligibleLoreIds(db(), 7, true);
    std::set<std::string> seen;
    for (int count = 0; count < static_cast<int>(pool.size()); ++count) {
        const auto e = makeLoreEncounter(db(), 7, true, 4242ull, 10 + count, count);
        REQUIRE(e.has_value());
        CHECK(seen.insert(e->questionId).second);
    }
    CHECK(seen.size() == pool.size());
    // The cycle wraps, and the same (seed, count) reproduces the same pick.
    const auto wrap = makeLoreEncounter(db(), 7, true, 4242ull, 99, static_cast<int>(pool.size()));
    const auto first = makeLoreEncounter(db(), 7, true, 4242ull, 10, 0);
    REQUIRE(wrap.has_value());
    REQUIRE(first.has_value());
    CHECK(wrap->questionId == first->questionId);
    // A different seed walks a different order (with overwhelming likelihood
    // over 28 entries; pinned on one pair).
    const auto other = makeLoreEncounter(db(), 7, true, 4243ull, 10, 0);
    REQUIRE(other.has_value());
    bool differs = other->questionId != first->questionId;
    for (int c = 1; c < 4 && !differs; ++c) {
        differs = makeLoreEncounter(db(), 7, true, 4243ull, 10 + c, c)->questionId !=
                  makeLoreEncounter(db(), 7, true, 4242ull, 10 + c, c)->questionId;
    }
    CHECK(differs);
}

TEST_CASE("lore: two answers, the right one on either side, seeded by the patrol", "[lore]") {
    int top = 0;
    int bottom = 0;
    for (int patrol = 0; patrol < 64; ++patrol) {
        const auto e = makeLoreEncounter(db(), 7, true, 77ull, patrol, patrol);
        REQUIRE(e.has_value());
        const content::LoreQuestionDef* q = db().findLoreQuestion(e->questionId);
        REQUIRE(q != nullptr);
        CHECK(e->answers[static_cast<std::size_t>(e->correctAnswer)] == q->answer);
        CHECK(e->answers[static_cast<std::size_t>(1 - e->correctAnswer)] == q->wrongAnswer);
        CHECK(e->question == q->question);
        CHECK(e->mockLine == q->mockLine);
        (e->correctAnswer == 0 ? top : bottom)++;
    }
    CHECK(top > 8);
    CHECK(bottom > 8);
    // An empty pool (nothing eligible) is a nullopt, not a crash.
    content::ContentDatabase empty;
    CHECK_FALSE(makeLoreEncounter(empty, 7, true, 1ull, 0, 0).has_value());
}

TEST_CASE("lore: the field is the Jester and two answers, alive but inert", "[lore]") {
    SpecialEncounter e;
    e.kind = SpecialKind::Lore;
    e.lore = *makeLoreEncounter(db(), 3, false, 5ull, 2, 0);
    battle::Battle b = battle::buildBattle(makeParty(), dungeon::EnemyTeam{}, db());
    const int first = appendPlaceholders(b, e);
    REQUIRE(first == 4);
    REQUIRE(b.units.size() == 7);
    CHECK(b.units[4].sourceId == std::string(kJesterSourceId));
    CHECK(b.units[4].name == "The Jester");
    CHECK(b.units[5].name == e.lore.answers[0]);
    CHECK(b.units[6].name == e.lore.answers[1]);
    for (int i = first; i < 7; ++i) {
        CHECK(b.units[static_cast<std::size_t>(i)].side == battle::Side::Enemy);
        CHECK(b.units[static_cast<std::size_t>(i)].hp == 1);
        CHECK(b.units[static_cast<std::size_t>(i)].alive());
        CHECK(b.units[static_cast<std::size_t>(i)].isBoss == false);
    }
    // The model sees a living enemy side (so no instant victory) ...
    CHECK(b.outcome() == battle::Outcome::Ongoing);
    CHECK(b.aliveIndices(battle::Side::Enemy).size() == 3);
    // ... and the screen's turn order without them is the party alone.
    const std::vector<int> order = orderAfterDecision(b, -1);
    CHECK(order.size() == 7);  // the pure helper removes only the decider
}

TEST_CASE("lore: the resolution rule", "[lore]") {
    SpecialEncounter e;
    e.kind = SpecialKind::Lore;
    e.lore = *makeLoreEncounter(db(), 7, true, 9ull, 1, 3);
    const int right = 1 + e.lore.correctAnswer;
    const int wrong = 1 + (1 - e.lore.correctAnswer);

    SpecialEncounter a = e;
    CHECK(resolveSpecial(a, right, false) == SpecialResult::LoreCorrect);
    CHECK(a.rewardGold == kLoreReward);
    CHECK(a.resolved());
    CHECK(specialRewards(a.result));

    SpecialEncounter w = e;
    CHECK(resolveSpecial(w, wrong, false) == SpecialResult::LoreWrong);
    CHECK(w.rewardGold == 0);
    CHECK(specialMocks(w.result));
    CHECK_FALSE(specialPunishes(w.result));

    SpecialEncounter j = e;
    CHECK(resolveSpecial(j, 0, false) == SpecialResult::JesterPunished);
    CHECK(specialPunishes(j.result));
    CHECK(specialMocks(j.result));
    CHECK(j.rewardGold == 0);

    SpecialEncounter s = e;
    CHECK(resolveSpecial(s, right, true) == SpecialResult::AoePunished);  // a sweep, even of the right answer
    CHECK(specialPunishes(s.result));

    CHECK(specialPrompt(e) == e.lore.question);
}

TEST_CASE("lore: the one AOE definition is the battle's own targeting", "[lore][chest]") {
    SpecialEncounter e;
    e.kind = SpecialKind::Chests;
    battle::Battle b = battle::buildBattle(makeParty(), dungeon::EnemyTeam{}, db());
    appendPlaceholders(b, e);
    // A basic attack: one target for everyone but a sweeping class.
    CHECK(b.hostileTargetCount(0, nullptr) == 1);
    battle::Combatant& knight = b.units[0];
    knight.attackHitsAll = true;  // the Dragon's sweep
    CHECK(b.hostileTargetCount(0, nullptr) == 3);
    knight.attackHitsAll = false;
    // Skills by their authored target.
    int allEnemies = 0;
    int single = 0;
    int allies = 0;
    for (const auto& [id, s] : db().skills()) {
        const int n = b.hostileTargetCount(0, &s);
        switch (s.target) {
            case content::SkillTarget::AllEnemies:
                CHECK(n == 3);
                ++allEnemies;
                break;
            case content::SkillTarget::SingleEnemy:
                CHECK(n == 1);
                ++single;
                break;
            default:
                CHECK(n == 0);  // ally-facing and self skills hit no foe
                ++allies;
                break;
        }
    }
    CHECK(allEnemies > 0);
    CHECK(single > 0);
    CHECK(allies > 0);
    // Offensive == enemy-facing, the command menu's filter.
    for (const auto& [id, s] : db().skills()) {
        CHECK(skillIsOffensive(s) == (s.target == content::SkillTarget::SingleEnemy ||
                                      s.target == content::SkillTarget::AllEnemies));
    }
}
