#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "content/LoadReport.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/TextLayout.hpp"

using cd::content::LoadReport;
using cd::tutorial::Beat;
using cd::tutorial::Progress;
using cd::tutorial::TutorialStore;

TEST_CASE("tutorial progress round-trips through serialize/parse", "[tutorial]") {
    Progress p;
    p.enabled = false;
    p.seen = {"town_welcome", "battle_first", "some_future_beat"};
    const std::string text = cd::tutorial::serializeTutorial(p);

    Progress back;
    LoadReport report;
    REQUIRE(cd::tutorial::parseTutorialText(text, back, report));
    CHECK(back.enabled == false);
    CHECK(back.seen == p.seen);  // unknown ids survive the round trip
    CHECK(report.errorCount() == 0);
}

TEST_CASE("malformed tutorial state yields a fresh default, never a crash", "[tutorial]") {
    Progress p;
    LoadReport report;

    SECTION("not JSON") { CHECK_FALSE(cd::tutorial::parseTutorialText("{oops", p, report)); }
    SECTION("not an object") { CHECK_FALSE(cd::tutorial::parseTutorialText("[1,2]", p, report)); }
    SECTION("wrong version") {
        CHECK_FALSE(cd::tutorial::parseTutorialText(R"({"version": 99, "seen": []})", p, report));
    }
    SECTION("missing version") {
        CHECK_FALSE(cd::tutorial::parseTutorialText(R"({"seen": ["x"]})", p, report));
    }
    // Every failure leaves the safe default: enabled, nothing seen.
    CHECK(p.enabled == true);
    CHECK(p.seen.empty());
    CHECK(report.errorCount() > 0);
}

TEST_CASE("non-string seen entries are skipped, valid ones kept", "[tutorial]") {
    Progress p;
    LoadReport report;
    REQUIRE(cd::tutorial::parseTutorialText(
        R"({"version": 1, "enabled": true, "seen": ["a", 7, null, "b"]})", p, report));
    CHECK(p.seen == std::set<std::string>{"a", "b"});
}

TEST_CASE("tutorial store: take-once, disable, reset, missing file", "[tutorial]") {
    const std::filesystem::path file =
        std::filesystem::temp_directory_path() / "crystal_test_tutorial.json";
    std::filesystem::remove(file);

    TutorialStore store(file);
    LoadReport report;
    CHECK(store.load(report));  // missing file: silent fresh state
    CHECK(store.state.enabled);

    // First encounter fires and persists; the second never does.
    CHECK(store.takeBeat("town_welcome"));
    CHECK_FALSE(store.takeBeat("town_welcome"));

    // A fresh store over the same file sees the persisted progress.
    TutorialStore again(file);
    CHECK(again.load(report));
    CHECK_FALSE(again.takeBeat("town_welcome"));

    // Disabled prompts never fire (and are not marked seen).
    again.state.enabled = false;
    CHECK_FALSE(again.takeBeat("battle_first"));
    CHECK(again.state.seen.count("battle_first") == 0);

    // Reset clears seen beats but keeps the enabled flag.
    again.state.enabled = true;
    again.reset();
    CHECK(again.takeBeat("town_welcome"));

    std::filesystem::remove(file);
}

TEST_CASE("beat table: unique ids, findable, and panel-safe text", "[tutorial]") {
    // Conservative measure: the raylib default font is ~6px/char at size 10;
    // 7px overestimates, so a pass here means the real render fits too.
    const cd::ui::TextMeasure measure = [](const std::string& text, int fontSize) {
        return static_cast<int>(text.size()) * (fontSize * 7) / 10;
    };
    constexpr int kPromptTextW = 320 - 16;  // TutorialPromptState panel minus padding
    constexpr int kMaxLines = 12;           // keeps the tallest panel well inside 240

    std::set<std::string> ids;
    for (const Beat& b : cd::tutorial::kBeats) {
        INFO(b.id);
        CHECK(ids.insert(b.id).second);
        CHECK(cd::tutorial::findBeat(b.id) == &b);
        REQUIRE(b.title != nullptr);
        REQUIRE(b.body != nullptr);
        CHECK(std::string(b.title).size() > 0);
        const auto lines = cd::ui::wrapText(b.body, kPromptTextW, 10, measure);
        CHECK(lines.size() >= 2);
        CHECK(lines.size() <= kMaxLines);
        for (const std::string& line : lines) {
            CHECK(measure(line, 10) <= kPromptTextW);
        }
    }
    CHECK(ids.size() == cd::tutorial::kBeatCount);
    CHECK(cd::tutorial::findBeat("no_such_beat") == nullptr);
}
