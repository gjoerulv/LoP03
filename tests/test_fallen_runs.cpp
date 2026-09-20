// M124 - the Hall of Shame: the fallen-run codec and store (round trip, the
// newest-20 cap, malformed / foreign files, bad records), the party snapshot
// travelling through the slot codec (SaveSystem::serialize / parseText), the
// fallen summary's lead rows, and the send-off's phrase pool.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "game/Character.hpp"
#include "game/FallenRuns.hpp"
#include "game/Party.hpp"
#include "game/Summary.hpp"
#include "save/SaveSystem.hpp"
#include "states/FallenPhrases.hpp"

using namespace cd;

namespace {

const content::ContentDatabase& db() {
    static const content::ContentDatabase loaded = [] {
        content::ContentDatabase d;
        content::LoadReport rep;
        content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), d, rep);
        REQUIRE(rep.ok());
        return d;
    }();
    return loaded;
}

std::filesystem::path tempDir() {
    static std::atomic<int> counter{0};
    return std::filesystem::temp_directory_path() /
           ("cd_fallen_" + std::to_string(counter.fetch_add(1)));
}

Party makeParty() {
    Party p;
    resetForNewGame(p);
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        const content::ClassDef* cls = db().findClass(id);
        REQUIRE(cls != nullptr);
        p.members.push_back(createCharacter(*cls, id, 7));
    }
    p.members[0].name = "Rolan";
    p.gold = 321;
    p.lifetime.explore.playSeconds = 3600 + 3 * 60 + 56;
    p.lifetime.combat.battlesWon = 12;
    return p;
}

FallenRun makeRun(int n) {
    FallenRun run;
    run.place = "Town " + std::to_string(n);
    run.foes = "Foe " + std::to_string(n);
    run.leader = "Leader" + std::to_string(n);
    run.highestLevel = n;
    run.playSeconds = 100LL * n;
    return run;
}

}  // namespace

TEST_CASE("m124: a fallen run round-trips the codec", "[fallenruns][m124]") {
    const std::filesystem::path dir = tempDir();
    save::SaveSystem saves(db(), dir);
    Party p = makeParty();
    p.ironMan = true;  // serialize has no Iron Man refusal - that is save()'s alone

    FallenRun run = makeRun(3);
    run.party = saves.serialize(p);
    REQUIRE_FALSE(run.party.empty());

    std::vector<FallenRun> out;
    content::LoadReport rep;
    REQUIRE(parseFallenRunsText(serializeFallenRuns({run}), out, rep));
    CHECK(rep.ok());
    REQUIRE(out.size() == 1);
    CHECK(out[0].place == "Town 3");
    CHECK(out[0].foes == "Foe 3");
    CHECK(out[0].leader == "Leader3");
    CHECK(out[0].highestLevel == 3);
    CHECK(out[0].playSeconds == 300);

    // The snapshot comes back as a party the slot codec accepts - the same
    // members, gold and ledger - and never as an Iron Man flag.
    Party back;
    content::LoadReport parseRep;
    REQUIRE(saves.parseText(out[0].party, "fallen_runs.json", back, parseRep));
    REQUIRE(back.members.size() == 4);
    CHECK(back.members[0].name == "Rolan");
    CHECK(back.members[0].level == 7);
    CHECK(back.gold == 321);
    CHECK(back.lifetime.explore.playSeconds == 3600 + 3 * 60 + 56);
    CHECK(back.lifetime.combat.battlesWon == 12);
    CHECK_FALSE(back.ironMan);

    // ...while the slot writer still refuses the very same party.
    content::LoadReport saveRep;
    CHECK_FALSE(saves.save(save::SaveSlot::Manual1, p, saveRep));
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("m124: the slot codec text is what save() writes", "[fallenruns][save][m124]") {
    const std::filesystem::path dir = tempDir();
    save::SaveSystem saves(db(), dir);
    const Party p = makeParty();
    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual2, p, rep));
    std::ifstream in(saves.slotPath(save::SaveSlot::Manual2));  // text mode: newline-agnostic
    REQUIRE(in);
    const std::string onDisk((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(onDisk == saves.serialize(p));  // the M124 extraction is behavior-neutral

    Party viaText;
    content::LoadReport textRep;
    REQUIRE(saves.parseText(onDisk, "test", viaText, textRep));
    Party viaSlot;
    content::LoadReport slotRep;
    REQUIRE(saves.load(save::SaveSlot::Manual2, viaSlot, slotRep));
    CHECK(saves.serialize(viaText) == saves.serialize(viaSlot));

    Party untouched = makeParty();
    untouched.gold = 5;
    content::LoadReport badRep;
    CHECK_FALSE(saves.parseText("{ not json", "test", untouched, badRep));
    CHECK_FALSE(badRep.ok());
    CHECK(untouched.gold == 5);  // a failed parse never touches the target
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("m124: the hall keeps the newest twenty, newest first", "[fallenruns][m124]") {
    std::vector<FallenRun> runs;
    for (int i = 1; i <= 25; ++i) {
        addFallenRun(runs, makeRun(i));
    }
    REQUIRE(runs.size() == kFallenRunsKept);
    CHECK(runs.front().place == "Town 25");
    CHECK(runs.back().place == "Town 6");

    // A file holding more than the cap (hand-edited) loads only the cap.
    std::vector<FallenRun> many;
    for (int i = 0; i < 30; ++i) {
        many.push_back(makeRun(i + 1));
    }
    std::vector<FallenRun> out;
    content::LoadReport rep;
    REQUIRE(parseFallenRunsText(serializeFallenRuns(many), out, rep));
    CHECK(out.size() == kFallenRunsKept);
    CHECK(out.front().place == "Town 1");
}

TEST_CASE("m124: malformed or foreign files read as an empty hall", "[fallenruns][m124]") {
    std::vector<FallenRun> out{makeRun(1)};
    content::LoadReport a;
    CHECK_FALSE(parseFallenRunsText("not json at all", out, a));
    CHECK(out.empty());
    CHECK_FALSE(a.ok());

    out = {makeRun(1)};
    content::LoadReport b;
    CHECK_FALSE(parseFallenRunsText("[1, 2, 3]", out, b));
    CHECK(out.empty());

    out = {makeRun(1)};
    content::LoadReport c;
    CHECK_FALSE(parseFallenRunsText(R"({"version": 99, "runs": []})", out, c));
    CHECK(out.empty());

    content::LoadReport d;
    CHECK(parseFallenRunsText(R"({"version": 1})", out, d));  // valid and empty
    CHECK(out.empty());
    CHECK(d.ok());
}

TEST_CASE("m124: one bad record is skipped, the rest are kept", "[fallenruns][m124]") {
    const std::string text = R"({
      "version": 1,
      "runs": [
        {"place": "Town 2", "foes": "A Goose", "leader": "Mira", "highestLevel": 9,
         "playSeconds": 77, "party": "not an object"},
        17,
        {},
        {"place": 5, "foes": "Only a foe", "highestLevel": -4, "playSeconds": "soon"}
      ]
    })";
    std::vector<FallenRun> out;
    content::LoadReport rep;
    REQUIRE(parseFallenRunsText(text, out, rep));
    CHECK_FALSE(rep.ok());  // the two skipped entries are reported
    REQUIRE(out.size() == 2);
    CHECK(out[0].place == "Town 2");
    CHECK(out[0].party.empty());  // a snapshot that is not an object is dropped
    CHECK(out[1].place.empty());  // wrong types read as defaults, never throw
    CHECK(out[1].foes == "Only a foe");
    CHECK(out[1].highestLevel == 0);
    CHECK(out[1].playSeconds == 0);

    // A snapshot that is not JSON never poisons the file either.
    FallenRun junk = makeRun(4);
    junk.party = "{ broken";
    std::vector<FallenRun> again;
    content::LoadReport rep2;
    REQUIRE(parseFallenRunsText(serializeFallenRuns({junk}), again, rep2));
    REQUIRE(again.size() == 1);
    CHECK(again[0].party.empty());
    CHECK(again[0].place == "Town 4");
}

TEST_CASE("m124: the store records, persists and survives a bad file", "[fallenruns][m124]") {
    const std::filesystem::path dir = tempDir();
    std::filesystem::create_directories(dir);
    const std::filesystem::path file = dir / "fallen_runs.json";

    FallenRunStore fresh(file);
    content::LoadReport rep;
    REQUIRE(fresh.load(rep));  // missing file: nobody has fallen yet
    CHECK(fresh.runs.empty());

    REQUIRE(fresh.record(makeRun(1), rep));
    REQUIRE(fresh.record(makeRun(2), rep));
    CHECK(fresh.runs.front().place == "Town 2");

    FallenRunStore reloaded(file);
    content::LoadReport rep2;
    REQUIRE(reloaded.load(rep2));
    REQUIRE(reloaded.runs.size() == 2);
    CHECK(reloaded.runs[0].place == "Town 2");
    CHECK(reloaded.runs[1].place == "Town 1");

    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "{{{{ definitely not json";
    }
    FallenRunStore broken(file);
    content::LoadReport rep3;
    CHECK_FALSE(broken.load(rep3));
    CHECK(broken.runs.empty());
    // ...and the next fall simply starts the hall over.
    content::LoadReport rep4;
    REQUIRE(broken.record(makeRun(9), rep4));
    FallenRunStore after(file);
    content::LoadReport rep5;
    REQUIRE(after.load(rep5));
    REQUIRE(after.runs.size() == 1);
    CHECK(after.runs[0].place == "Town 9");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("m124: a fallen summary leads with where and who", "[summary][m124]") {
    const std::vector<SummaryRow> rows =
        fallenSummaryRows("Town 3 - Crystal Mine, floor 2 of 4", "The Gate Wardens");
    REQUIRE(rows.size() == 6);
    CHECK(rows[0].header);
    CHECK(rows[0].label == "Fell at");
    CHECK_FALSE(rows[1].header);
    CHECK(rows[1].label == "Town 3 - Crystal Mine, floor 2 of 4");
    CHECK(rows[1].value.empty());  // full-width: a place line never fits the value column
    CHECK(rows[2].label == "Beaten by");
    CHECK(rows[3].label == "The Gate Wardens");
    CHECK(rows[5].label == "Mode");
    CHECK(rows[5].value == "Iron Man");

    const std::vector<SummaryRow> blank = fallenSummaryRows("", "");
    CHECK_FALSE(blank[1].label.empty());
    CHECK_FALSE(blank[3].label.empty());
}

TEST_CASE("m124: fallen phrases are non-empty, genre-free, and fit the screen",
          "[options][m124]") {
    // The celebration pool's lint: a conservative 7px/char over-estimate at
    // font 10, centred with an edge margin.
    constexpr int kMaxWidth = 426 - 32;
    for (const char* phrase : kFallenPhrases) {
        const std::string s = phrase;
        INFO(phrase);
        REQUIRE_FALSE(s.empty());
        CHECK(static_cast<int>(s.size()) * 7 <= kMaxWidth);
        for (const char ch : s) {
            CHECK(static_cast<unsigned char>(ch) < 128);  // ASCII: bound by the glyph table
        }
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const char* banned : {"roguelite", "roguelike", "rpg", "jrpg", "dungeon-score",
                                   "turn-based"}) {
            CHECK(lower.find(banned) == std::string::npos);
        }
    }
}
