// M80 — event flavor: the optional data/event_flavor.json, its loader rules,
// the content-side vocabulary held in lockstep with dungeon::RoomEventKind,
// and the panel's wrap budget refereed at authored length.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "dungeon/DungeonModel.hpp"
#include "dungeon/ThemeEvents.hpp"
#include "ui/TextLayout.hpp"
#include "ui/TextViewport.hpp"

using namespace cd;
namespace fs = std::filesystem;

namespace {

content::Json parse(const char* text) { return content::Json::parse(text, nullptr, false); }

const content::ContentDatabase& shipped() {
    static content::ContentDatabase db;
    static bool loaded = false;
    if (!loaded) {
        content::LoadReport rep;
        REQUIRE(content::loadAll(fs::path(CRYSTAL_TEST_DATA_DIR), db, rep));
        loaded = true;
    }
    return db;
}

}  // namespace

TEST_CASE("flavor: every event kind is authored, and nothing else is", "[flavor]") {
    CHECK(shipped().eventFlavorCount() == content::kEventFlavorIdCount);
    for (std::size_t i = 0; i < content::kEventFlavorIdCount; ++i) {
        const char* id = content::kEventFlavorIds[i];
        INFO(id);
        const content::EventFlavorDef* def = shipped().findEventFlavor(id);
        REQUIRE(def != nullptr);
        CHECK_FALSE(def->title.empty());
        CHECK_FALSE(def->body.empty());
    }
}

TEST_CASE("flavor: the dungeon mapping and the content vocabulary agree", "[flavor]") {
    // Every real RoomEventKind maps to a KNOWN flavor id (the two layers
    // cannot name each other; this test is the lockstep).
    using dungeon::RoomEventKind;
    const RoomEventKind kinds[] = {
        RoomEventKind::Shrine,       RoomEventKind::HealingSpring,
        RoomEventKind::Merchant,     RoomEventKind::EliteChallenge,
        RoomEventKind::ScoreWager,   RoomEventKind::RestToken,
        RoomEventKind::RoyalRelic,   RoomEventKind::ArmoryGhost,
        RoomEventKind::MinersCache,  RoomEventKind::ElderRoot,
        RoomEventKind::DuckPeddler,
    };
    static_assert(sizeof(kinds) / sizeof(kinds[0]) == content::kEventFlavorIdCount,
                  "a new RoomEventKind needs a flavor id (or an explicit exemption here)");
    for (RoomEventKind k : kinds) {
        const std::string id = dungeon::eventFlavorId(k);
        INFO(id);
        CHECK_FALSE(id.empty());
        bool known = false;
        for (std::size_t i = 0; i < content::kEventFlavorIdCount; ++i) {
            known = known || id == content::kEventFlavorIds[i];
        }
        CHECK(known);
    }
    CHECK(std::string(dungeon::eventFlavorId(RoomEventKind::None)).empty());
}

TEST_CASE("flavor: the panel CONTAINS every authored body (M87)", "[flavor]") {
    // M87 repealed the "body must fit 4 English lines" rule: the panel's body
    // is a bounded scrollable viewport (4 visible lines at 322px), so an
    // authored or translated body of ANY length must be fully reachable by
    // scrolling — that containment is what this test now referees. The TITLE
    // stays a fixed single line (policy A), so its width check remains.
    const cd::ui::TextMeasure measure = [](const std::string& text, int fontSize) {
        return static_cast<int>(text.size()) * (fontSize * 7) / 10;
    };
    constexpr int kPanelTextW = 322;   // 360 - 28 padding - 10 indicator gutter
    constexpr int kVisibleLines = 4;   // the panel's fixed body budget
    for (const auto& [id, def] : shipped().eventFlavors()) {
        INFO(id);
        CHECK(measure(def.title, 11) <= 332);
        cd::ui::TextViewport vp;
        vp.setContent(def.body, kPanelTextW, 10, measure);
        vp.setVisibleLines(kVisibleLines);
        REQUIRE(vp.lineCount() >= 1);
        // No line escapes the rectangle's width...
        for (int i = 0; i < vp.lineCount(); ++i) {
            CHECK(measure(vp.line(i), 10) <= kPanelTextW);
        }
        // ...and scrolling reaches the last line, then clamps.
        vp.scrollBy(vp.lineCount() * 2);
        CHECK_FALSE(vp.moreBelow());
        CHECK(vp.top() + vp.visibleCount() == vp.lineCount());
    }
}

TEST_CASE("flavor: the loader rejects the malformed and the unknown", "[flavor]") {
    SECTION("unknown id") {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseEventFlavor(parse(R"({"version":1,"events":[
            {"id":"disco_room","title":"T","body":"B"}]})"),
                                  "event_flavor.json", db, rep);
        CHECK_FALSE(rep.ok());
        CHECK(db.eventFlavorCount() == 0);
    }
    SECTION("duplicate id") {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseEventFlavor(parse(R"({"version":1,"events":[
            {"id":"shrine","title":"T","body":"B"},
            {"id":"shrine","title":"T2","body":"B2"}]})"),
                                  "event_flavor.json", db, rep);
        CHECK_FALSE(rep.ok());
        CHECK(db.eventFlavorCount() == 1);  // the first stands; the copy is refused
    }
    SECTION("missing title/body") {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseEventFlavor(parse(R"({"version":1,"events":[
            {"id":"shrine","body":"B"}]})"),
                                  "event_flavor.json", db, rep);
        CHECK_FALSE(rep.ok());
    }
    SECTION("a valid entry parses") {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseEventFlavor(parse(R"({"version":1,"events":[
            {"id":"shrine","title":"The Shrine","body":"A shrine."}]})"),
                                  "event_flavor.json", db, rep);
        CHECK(rep.ok());
        REQUIRE(db.findEventFlavor("shrine") != nullptr);
        CHECK(db.findEventFlavor("shrine")->title == "The Shrine");
    }
}

TEST_CASE("lint: TEST_CASE names are plain ASCII (ctest passes them by name)",
          "[flavor][lint]") {
    // ctest invokes each Catch2 case with its NAME as the filter; the Windows
    // codepage mangles any non-ASCII character in transit, the filter matches
    // nothing, and the case "fails" as No-Tests-Ran. This bit three separate
    // milestones (an em-dash each time) before this lint. Scans the test
    // sources themselves; the tests/ dir sits beside the data dir it knows.
    const fs::path testsDir = fs::path(CRYSTAL_TEST_DATA_DIR).parent_path() / "tests";
    REQUIRE(fs::exists(testsDir));
    int scanned = 0;
    for (const fs::directory_entry& e : fs::directory_iterator(testsDir)) {
        if (!e.is_regular_file() || e.path().extension() != ".cpp") {
            continue;
        }
        ++scanned;
        std::ifstream in(e.path(), std::ios::binary);
        std::string line;
        int lineNo = 0;
        bool inCase = false;  // TEST_CASE(...) headers may span two lines
        while (std::getline(in, line)) {
            ++lineNo;
            if (line.find("TEST_CASE") != std::string::npos) {
                inCase = true;
            }
            if (inCase) {
                for (unsigned char ch : line) {
                    INFO(e.path().filename().string() << ":" << lineNo << ": " << line);
                    CHECK(ch < 0x80);
                }
                if (line.find('{') != std::string::npos) {
                    inCase = false;  // the header ended (body braces follow)
                }
            }
        }
    }
    CHECK(scanned > 40);  // the sweep really saw the suite
}

TEST_CASE("flavor: the file is optional - deleting it blocks nothing", "[flavor]") {
    // A copy of the shipped JSONs WITHOUT event_flavor.json must load clean;
    // every event then falls back to its footer prompt (findEventFlavor
    // answers nullptr, the panel never opens).
    const fs::path tmp = fs::temp_directory_path() / "cd_flavorless_data";
    fs::create_directories(tmp);
    for (const fs::directory_entry& e : fs::directory_iterator(CRYSTAL_TEST_DATA_DIR)) {
        if (!e.is_regular_file() || e.path().extension() != ".json" ||
            e.path().filename() == "event_flavor.json") {
            continue;
        }
        fs::copy_file(e.path(), tmp / e.path().filename(),
                      fs::copy_options::overwrite_existing);
    }
    content::ContentDatabase db;
    content::LoadReport rep;
    INFO(rep.summary());
    CHECK(content::loadAll(tmp, db, rep));
    CHECK(db.eventFlavorCount() == 0);
    CHECK(db.findEventFlavor("shrine") == nullptr);
    std::error_code ec;
    fs::remove_all(tmp, ec);
}
