// M59 — CrystalForge's canonical writer against the SHIPPED data files.
// Three guarantees: (1) canonicalization never changes a value (documents are
// value-identical before/after), (2) the writer is idempotent and, after the
// one-time normalization, byte-stable against the files on disk, and (3) the
// real loader accepts the canonical bytes with zero errors.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "editor/CanonicalJson.hpp"
#include "editor/EditorDocs.hpp"
#include "editor/EditorValidation.hpp"

namespace {

namespace fs = std::filesystem;
using cd::editor::OrderedJson;

fs::path dataDir() { return fs::path(CRYSTAL_TEST_DATA_DIR); }

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    // Line-ending tolerant: a CRLF checkout (git autocrlf) must not fail the
    // byte-stability comparison against the writer's LF output.
    std::string text = buffer.str();
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
}

}  // namespace

TEST_CASE("editor: canonicalization preserves every value in every shipped file", "[editor]") {
    for (const cd::editor::CategoryInfo& info : cd::editor::categories()) {
        INFO(info.filename);
        const std::string original = readFile(dataDir() / info.filename);
        const OrderedJson doc = OrderedJson::parse(original, nullptr, false);
        REQUIRE_FALSE(doc.is_discarded());
        const std::string canon =
            cd::editor::canonicalize(doc, cd::editor::styleForFile(info.filename));
        // Value equality through order-insensitive plain json.
        const cd::content::Json before = cd::content::Json::parse(original);
        const cd::content::Json after = cd::content::Json::parse(canon);
        REQUIRE(before == after);
    }
}

TEST_CASE("editor: canonical writer is idempotent and byte-stable on disk", "[editor]") {
    for (const cd::editor::CategoryInfo& info : cd::editor::categories()) {
        INFO(info.filename);
        const std::string original = readFile(dataDir() / info.filename);
        const OrderedJson doc = OrderedJson::parse(original, nullptr, false);
        REQUIRE_FALSE(doc.is_discarded());
        const cd::editor::FileStyle style = cd::editor::styleForFile(info.filename);
        const std::string canon = cd::editor::canonicalize(doc, style);
        // Idempotent: re-parsing the canonical text reproduces it exactly.
        const OrderedJson reparsed = OrderedJson::parse(canon, nullptr, false);
        REQUIRE_FALSE(reparsed.is_discarded());
        REQUIRE(cd::editor::canonicalize(reparsed, style) == canon);
        // Byte-stable against the shipped file (the M59 one-time normalization
        // ran; from here on, editor saves can never reformat a file).
        REQUIRE(canon == original);
    }
}

TEST_CASE("editor: the real loader accepts the shipped files with zero errors", "[editor]") {
    cd::editor::EditorDocs docs;
    REQUIRE(docs.loadAll(dataDir()));
    cd::content::ContentDatabase db;
    cd::content::LoadReport rep;
    const bool ok = cd::editor::buildDatabase(docs, db, rep);
    INFO(rep.summary());
    REQUIRE(ok);
    // The scratch database matches a direct loadAll of the same files.
    cd::content::ContentDatabase direct;
    cd::content::LoadReport directRep;
    REQUIRE(cd::content::loadAll(dataDir(), direct, directRep));
    REQUIRE(db.skillCount() == direct.skillCount());
    REQUIRE(db.classCount() == direct.classCount());
    REQUIRE(db.enemyCount() == direct.enemyCount());
    REQUIRE(db.itemCount() == direct.itemCount());
    REQUIRE(db.bossCount() == direct.bossCount());
    REQUIRE(db.themeCount() == direct.themeCount());
    REQUIRE(db.passiveCount() == direct.passiveCount());
    REQUIRE(db.storyCount() == direct.storyCount());
    // M86: the two optional files are editor categories now — same equivalence.
    REQUIRE(db.eventFlavorCount() == direct.eventFlavorCount());
    REQUIRE(db.curioLoreCount() == direct.curioLoreCount());
    REQUIRE(db.curioLoreCount() > 0);
}

TEST_CASE("editor: quick checks run and pass on the shipped content", "[editor]") {
    cd::editor::EditorDocs docs;
    REQUIRE(docs.loadAll(dataDir()));
    const cd::editor::ValidationResult result = cd::editor::validateDocs(docs);
    INFO(result.report.summary());
    REQUIRE(result.databaseOk);
    REQUIRE(result.checks.size() == 3);
    for (const cd::editor::QuickCheckResult& check : result.checks) {
        INFO(check.name + " - " + check.detail);
        CHECK(check.ran);
        CHECK(check.passed);
    }
}

TEST_CASE("editor: unknown keys survive a canonical round-trip", "[editor]") {
    const std::string text = R"({
  "version": 1,
  "skills": [
    { "id": "test", "name": "Test", "category": "physical", "target": "single_enemy", "futureKey": [1, 2] }
  ]
})";
    const OrderedJson doc = OrderedJson::parse(text);
    const std::string canon =
        cd::editor::canonicalize(doc, cd::editor::FileStyle::InlineEntities);
    REQUIRE(canon.find("\"futureKey\": [1, 2]") != std::string::npos);
}

// ---- M125: the M120/M121 fields through the editor's own pipeline ------------

namespace {
cd::editor::OrderedJson* findById(cd::editor::EditorDocs& docs, cd::editor::Category category,
                                  const std::string& id) {
    for (int i = 0; i < docs.entityCount(category); ++i) {
        cd::editor::OrderedJson* entity = docs.entityAt(category, i);
        if (entity != nullptr && entity->value("id", std::string{}) == id) {
            return entity;
        }
    }
    return nullptr;
}
}  // namespace

TEST_CASE("editor: an edited MP rider validates through the real loader (M125)",
          "[editor][m125]") {
    cd::editor::EditorDocs docs;
    REQUIRE(docs.loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR)));

    // A designer raises the Elixir's rider: still a clean database, and the
    // loaded item carries the edited number.
    cd::editor::OrderedJson* elixir = findById(docs, cd::editor::Category::Items, "elixir");
    REQUIRE(elixir != nullptr);
    (*elixir)["mpAmount"] = 75;
    {
        cd::content::ContentDatabase db;
        cd::content::LoadReport rep;
        REQUIRE(cd::editor::buildDatabase(docs, db, rep));
        const cd::content::ItemDef* item = db.findItem("elixir");
        REQUIRE(item != nullptr);
        CHECK(item->mpAmount == 75);
    }

    // The same rider on something that is not a healing consumable is the
    // loader's error, surfaced by the editor instead of written to disk.
    cd::editor::OrderedJson* antidote = findById(docs, cd::editor::Category::Items, "antidote");
    REQUIRE(antidote != nullptr);
    (*antidote)["mpAmount"] = 10;
    const cd::editor::ValidationResult bad = cd::editor::validateDocs(docs);
    CHECK_FALSE(bad.databaseOk);
    CHECK_FALSE(bad.report.ok());
}

TEST_CASE("editor: skill texts validate, reject unknown skills, and round-trip (M125)",
          "[editor][m125]") {
    cd::editor::EditorDocs docs;
    REQUIRE(docs.loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR)));
    const cd::editor::CategoryInfo& info = cd::editor::infoFor(cd::editor::Category::Milestones);

    cd::editor::OrderedJson* milestone = nullptr;
    for (int i = 0; i < docs.entityCount(cd::editor::Category::Milestones); ++i) {
        cd::editor::OrderedJson* m = docs.entityAt(cd::editor::Category::Milestones, i);
        if (m != nullptr && m->contains("skillTexts")) {
            milestone = m;
            break;
        }
    }
    REQUIRE(milestone != nullptr);

    // Canonical writing keeps the array, its order and its text exactly.
    const cd::editor::OrderedJson before = (*milestone)["skillTexts"];
    const std::string canon = cd::editor::canonicalize(docs.file(info.category).root,
                                                       cd::editor::styleForFile(info.filename));
    const cd::editor::OrderedJson reparsed = cd::editor::OrderedJson::parse(canon);
    bool matched = false;
    for (const cd::editor::OrderedJson& m : reparsed[info.arrayKey]) {
        if (m.value("id", std::string{}) == milestone->value("id", std::string{})) {
            matched = true;
            CHECK(m["skillTexts"] == before);
        }
    }
    CHECK(matched);

    // A second rewrite of a skill this milestone already rewrites is the
    // loader's duplicate error.
    REQUIRE_FALSE(before.empty());
    (*milestone)["skillTexts"].push_back(before[0]);
    {
        cd::content::ContentDatabase db;
        cd::content::LoadReport rep;
        CHECK_FALSE(cd::editor::buildDatabase(docs, db, rep));
    }

    // A rewrite naming a skill that does not exist is a reference error.
    (*milestone)["skillTexts"] = before;
    cd::editor::OrderedJson ghost = cd::editor::OrderedJson::object();
    ghost["skill"] = "no_such_skill";
    ghost["description"] = "Does nothing, to nobody.";
    (*milestone)["skillTexts"].push_back(ghost);
    const cd::editor::ValidationResult bad = cd::editor::validateDocs(docs);
    CHECK_FALSE(bad.databaseOk);

    // ...and with the ghost removed the documents are clean again.
    (*milestone)["skillTexts"] = before;
    cd::content::ContentDatabase db;
    cd::content::LoadReport rep;
    CHECK(cd::editor::buildDatabase(docs, db, rep));
}
