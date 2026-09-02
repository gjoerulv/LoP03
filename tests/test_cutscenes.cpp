// M97 — the Hooded Goose cutscenes: the loader's shape/vocabulary rules, the
// cross-file heirloom rules (exist, be heirlooms, one owner each), the shipped
// eight-scene arc's coverage pins, the pure story-progress helpers, and the
// save round-trip of seen scenes + recorded choices.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "game/Cutscenes.hpp"
#include "game/Party.hpp"
#include "save/SaveSystem.hpp"

using namespace cd;

namespace {

content::Json parse(const char* text) { return content::Json::parse(text, nullptr, false); }

const content::ContentDatabase& db() {
    static content::ContentDatabase database = [] {
        content::ContentDatabase d;
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), d, rep));
        return d;
    }();
    return database;
}

// A minimal valid scene body around the piece under test.
std::string sceneJson(const char* id, const char* beatExtra, const char* heirloomA,
                      const char* heirloomB) {
    std::string s = R"({"version":1,"cutscenes":[{"id":")";
    s += id;
    s += R"(","question":"Pick.","beats":[{"speaker":"S","text":"T")";
    s += beatExtra;
    s += R"(}],"options":[
        {"label":"A","heirloom":")";
    s += heirloomA;
    s += R"(","responseSpeaker":"S","responseText":"RA"},
        {"label":"B","heirloom":")";
    s += heirloomB;
    s += R"(","responseSpeaker":"S","responseText":"RB"}]}]})";
    return s;
}

bool parses(const std::string& json) {
    content::ContentDatabase mem;
    content::LoadReport rep;
    content::parseCutscenes(parse(json.c_str()), "mem", mem, rep);
    return rep.ok();
}

Party fourParty() {
    Party p;
    p.members.push_back(createCharacter(*db().findClass("knight"), "Rolan", 5));
    p.members.push_back(createCharacter(*db().findClass("mage"), "Mira", 5));
    p.members.push_back(createCharacter(*db().findClass("cleric"), "Sable", 5));
    p.members.push_back(createCharacter(*db().findClass("rogue"), "Pell", 5));
    return p;
}

}  // namespace

// --- loader shape & vocabulary ----------------------------------------------

TEST_CASE("cutscenes: the loader owns id, emote, beat and option shape", "[cutscene]") {
    CHECK(parses(sceneJson("new_game", "", "heirloom_a", "heirloom_b")));
    CHECK(parses(sceneJson("finale", R"(,"emote":"panic","kingOnStage":true)", "a", "b")));
    // Unknown scene ids and emotes are rejected — a typo cannot author a
    // scene no trigger will ever play, or an act the stage cannot do.
    CHECK_FALSE(parses(sceneJson("town_8", "", "a", "b")));
    CHECK_FALSE(parses(sceneJson("new_game", R"(,"emote":"moonwalk")", "a", "b")));
    // Structural rules: beats required, exactly two options.
    CHECK_FALSE(parses(R"({"version":1,"cutscenes":[{"id":"new_game","question":"Q",
        "options":[{"label":"A","heirloom":"a","responseSpeaker":"S","responseText":"R"},
                   {"label":"B","heirloom":"b","responseSpeaker":"S","responseText":"R"}]}]})"));
    CHECK_FALSE(parses(R"({"version":1,"cutscenes":[{"id":"new_game","question":"Q",
        "beats":[{"speaker":"S","text":"T"}],
        "options":[{"label":"A","heirloom":"a","responseSpeaker":"S","responseText":"R"}]}]})"));
    // Duplicates are rejected like every other category.
    content::ContentDatabase mem;
    content::LoadReport rep;
    content::parseCutscenes(parse(sceneJson("town_2", "", "a", "b").c_str()), "mem", mem, rep);
    content::parseCutscenes(parse(sceneJson("town_2", "", "c", "d").c_str()), "mem", mem, rep);
    CHECK_FALSE(rep.ok());
    CHECK(mem.cutsceneCount() == 1);
}

TEST_CASE("cutscenes: options must grant real heirlooms, one owner each", "[cutscene]") {
    const char* kHeirloom = R"({"version":1,"items":[
        {"id":"keep_a","name":"A","type":"heirloom","slot":"heirloom","value":0},
        {"id":"keep_b","name":"B","type":"heirloom","slot":"heirloom","value":0},
        {"id":"plain_sword","name":"S","type":"equipment","slot":"weapon",
         "iconCategory":"sword"}]})";
    const auto refErrors = [&](const std::string& scenes) {
        content::ContentDatabase mem;
        content::LoadReport rep;
        content::parseItems(parse(kHeirloom), "mem", mem, rep);
        content::parseCutscenes(parse(scenes.c_str()), "mem", mem, rep);
        REQUIRE(rep.ok());  // shape is fine; the cross-file pass decides
        content::validateReferences(mem, rep);
        return !rep.ok();
    };
    CHECK_FALSE(refErrors(sceneJson("new_game", "", "keep_a", "keep_b")));
    CHECK(refErrors(sceneJson("new_game", "", "keep_a", "no_such_item")));
    CHECK(refErrors(sceneJson("new_game", "", "keep_a", "plain_sword")));
    // The same keepsake offered twice — even inside one scene — is an error.
    CHECK(refErrors(sceneJson("new_game", "", "keep_a", "keep_a")));
}

// --- the shipped arc ---------------------------------------------------------

#ifdef CRYSTAL_TEST_DATA_DIR
TEST_CASE("cutscenes: the shipped arc is eight scenes covering all 16 heirlooms",
          "[cutscene][data]") {
    // All 8 arc scenes, plus only the M100 joke and M103 story pools.
    REQUIRE(db().cutsceneCount() == content::kCutsceneIdCount +
                                        game::strangerJokeIds(db()).size() +
                                        game::strangerStoryIds(db()).size());
    std::set<std::string> granted;
    for (std::size_t i = 0; i < content::kCutsceneIdCount; ++i) {
        const content::CutsceneDef* scene = db().findCutscene(content::kCutsceneIds[i]);
        INFO(content::kCutsceneIds[i]);
        REQUIRE(scene != nullptr);
        CHECK(scene->beats.size() >= 6);  // semi-long dialogues, per the owner
        REQUIRE(scene->options.size() == 2);
        CHECK_FALSE(scene->question.empty());
        for (const content::CutsceneOption& o : scene->options) {
            const content::ItemDef* item = db().findItem(o.heirloomId);
            REQUIRE(item != nullptr);
            CHECK(item->type == content::ItemType::Heirloom);
            granted.insert(o.heirloomId);
        }
    }
    // 8 scenes x 2 options = every shipped heirloom exactly once (uniqueness
    // is a validateReferences rule; 16 distinct ids proves full coverage).
    CHECK(granted.size() == 16);
    // The finale stages the principals; the owner's two named heirlooms open
    // the story. Both are capture anchors too (scenes 112-114).
    const content::CutsceneDef* finale = db().findCutscene("finale");
    bool staged = false;
    for (const content::CutsceneBeat& b : finale->beats) {
        staged = staged || (b.kingOnStage && b.dragonOnStage);
    }
    CHECK(staged);
    const content::CutsceneDef* prologue = db().findCutscene("new_game");
    CHECK(prologue->options[0].heirloomId == "heirloom_emberwake");
    CHECK(prologue->options[1].heirloomId == "heirloom_lastlight");
}
#endif

// --- the pure story-progress rules ------------------------------------------

TEST_CASE("cutscenes: town mapping, seen marks and recorded choices", "[cutscene]") {
    CHECK(game::townCutsceneId(1).empty());   // town 1 is the prologue's stage
    CHECK(game::townCutsceneId(2) == "town_2");
    CHECK(game::townCutsceneId(7) == "town_7");
    CHECK(game::townCutsceneId(8).empty());

    Party p;
    CHECK_FALSE(game::cutsceneSeen(p, "new_game"));
    game::markCutsceneSeen(p, "new_game");
    game::markCutsceneSeen(p, "new_game");  // idempotent
    CHECK(game::cutsceneSeen(p, "new_game"));
    CHECK(p.seenCutscenes.size() == 1);

    // Choices record once; a second answer never overwrites the first.
    CHECK(game::cutsceneChoiceFor(p, "new_game").empty());
    game::recordCutsceneChoice(p, "new_game", "heirloom_emberwake");
    game::recordCutsceneChoice(p, "new_game", "heirloom_lastlight");
    CHECK(game::cutsceneChoiceFor(p, "new_game") == "heirloom_emberwake");
    CHECK(p.heirloomChoices.size() == 1);

    // The encoding is unambiguous and malformed entries decode to nothing.
    std::string scene;
    std::string keep;
    CHECK(game::decodeCutsceneChoice("town_2:heirloom_vowknot", scene, keep));
    CHECK(scene == "town_2");
    CHECK(keep == "heirloom_vowknot");
    CHECK_FALSE(game::decodeCutsceneChoice("no_colon_here", scene, keep));
    CHECK_FALSE(game::decodeCutsceneChoice(":dangling", scene, keep));
    CHECK_FALSE(game::decodeCutsceneChoice("dangling:", scene, keep));
}

TEST_CASE("cutscenes: name tokens resolve to the roster, defensively", "[cutscene]") {
    Party p = fourParty();
    CHECK(game::cutsceneResolveTokens("{member1} and {member4} nod.", p) ==
          "Rolan and Pell nod.");
    CHECK(game::cutsceneResolveTokens("{member2}, {member2}!", p) == "Mira, Mira!");
    // Unknown tokens and out-of-roster indices stay literal — never a splice.
    CHECK(game::cutsceneResolveTokens("{member9} {narrator}", p) == "{member9} {narrator}");
    Party solo;
    solo.members.push_back(createCharacter(*db().findClass("knight"), "Rolan", 1));
    CHECK(game::cutsceneResolveTokens("{member1} met {member2}.", solo) ==
          "Rolan met {member2}.");
}

// --- persistence -------------------------------------------------------------

#ifdef CRYSTAL_TEST_DATA_DIR
TEST_CASE("cutscenes: story progress round-trips; junk degrades to unseen",
          "[cutscene][save]") {
    Party p = fourParty();
    game::markCutsceneSeen(p, "new_game");
    game::markCutsceneSeen(p, "town_2");
    game::recordCutsceneChoice(p, "new_game", "heirloom_emberwake");
    // Hand-edited junk rides along: an unknown scene, an unknown keepsake,
    // and a malformed entry. All three must drop on load, silently.
    p.seenCutscenes.push_back("town_99");
    p.heirloomChoices.push_back("town_99:heirloom_vowknot");
    p.heirloomChoices.push_back("not_an_entry");

    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "crystal_cutscene_save_test";
    std::filesystem::remove_all(dir);
    save::SaveSystem saves(db(), dir);
    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep));
    CHECK(game::cutsceneSeen(loaded, "new_game"));
    CHECK(game::cutsceneSeen(loaded, "town_2"));
    CHECK(loaded.seenCutscenes.size() == 2);
    CHECK(game::cutsceneChoiceFor(loaded, "new_game") == "heirloom_emberwake");
    CHECK(loaded.heirloomChoices.size() == 1);
    // An old save simply has neither field: a fresh story.
    Party old;
    old.members = p.members;
    old.seenCutscenes.clear();
    old.heirloomChoices.clear();
    REQUIRE(saves.save(save::SaveSlot::Manual2, old, rep));
    Party fresh;
    REQUIRE(saves.load(save::SaveSlot::Manual2, fresh, rep));
    CHECK(fresh.seenCutscenes.empty());
    CHECK(fresh.heirloomChoices.empty());
    std::filesystem::remove_all(dir);
}
#endif

// --- M100: the post-finale joke pool -----------------------------------------

TEST_CASE("cutscenes: joke scenes are optionless tales, story scenes are not",
          "[cutscene]") {
    // A joke: known by prefix, no question, no options — loads clean.
    CHECK(parses(R"({"version":1,"cutscenes":[{"id":"joke_9",
        "beats":[{"speaker":"S","text":"T"}]}]})"));
    // A joke smuggling options is rejected (it must never grant).
    CHECK_FALSE(parses(R"({"version":1,"cutscenes":[{"id":"joke_9","question":"Q",
        "beats":[{"speaker":"S","text":"T"}],
        "options":[{"label":"A","heirloom":"a","responseSpeaker":"S","responseText":"R"},
                   {"label":"B","heirloom":"b","responseSpeaker":"S","responseText":"R"}]}]})"));
    // A story scene without its question is rejected.
    CHECK_FALSE(parses(R"({"version":1,"cutscenes":[{"id":"new_game",
        "beats":[{"speaker":"S","text":"T"}],
        "options":[{"label":"A","heirloom":"a","responseSpeaker":"S","responseText":"R"},
                   {"label":"B","heirloom":"b","responseSpeaker":"S","responseText":"R"}]}]})"));
    // The bare prefix alone is not an id.
    CHECK_FALSE(parses(R"({"version":1,"cutscenes":[{"id":"joke_",
        "beats":[{"speaker":"S","text":"T"}]}]})"));
}

#ifdef CRYSTAL_TEST_DATA_DIR
TEST_CASE("cutscenes: the shipped joke pool cycles, dry and rewardless",
          "[cutscene][data]") {
    const std::vector<std::string> jokes = game::strangerJokeIds(db());
    REQUIRE(jokes.size() >= 7);  // the M100 authored pool
    for (const std::string& id : jokes) {
        INFO(id);
        const content::CutsceneDef* j = db().findCutscene(id);
        REQUIRE(j != nullptr);
        CHECK(j->options.empty());   // never a grant
        CHECK(!j->beats.empty());
        for (const content::CutsceneBeat& b : j->beats) {
            CHECK(b.speaker == "THE STRANGER \"P\"");  // the M100 rename
        }
    }
    // The cycle: every joke heard once before any repeats, then it wraps.
    std::set<std::string> heard;
    for (int told = 0; told < static_cast<int>(jokes.size()); ++told) {
        heard.insert(game::nextStrangerJokeId(db(), told));
    }
    CHECK(heard.size() == jokes.size());
    CHECK(game::nextStrangerJokeId(db(), static_cast<int>(jokes.size())) ==
          game::nextStrangerJokeId(db(), 0));
    // The owner's own line ships verbatim.
    const content::CutsceneDef* first = db().findCutscene("joke_1");
    REQUIRE(first != nullptr);
    bool wildGoose = false;
    for (const content::CutsceneBeat& b : first->beats) {
        wildGoose = wildGoose || b.text.find("wild goose chase") != std::string::npos;
    }
    CHECK(wildGoose);
}

TEST_CASE("cutscenes: no scene still speaks as the un-named Stranger (M100)",
          "[cutscene][data]") {
    for (const auto& [id, scene] : db().cutscenes()) {
        INFO(id);
        for (const content::CutsceneBeat& b : scene.beats) {
            CHECK(b.speaker != "The Stranger");
        }
        for (const content::CutsceneOption& o : scene.options) {
            CHECK(o.responseSpeaker != "The Stranger");
        }
    }
}
#endif
