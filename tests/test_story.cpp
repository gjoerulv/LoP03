#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "game/Castle.hpp"  // kCastleTown
#include "game/Story.hpp"
#include "ui/TextLayout.hpp"

// M41 story serial: the shipped content validates, every beat fits the dialog
// panel, and the read-progress mask helpers behave. Tone / whether the jokes land
// is an owner manual-validation item.

using namespace cd;

namespace {
content::ContentDatabase loadContent() {
    content::ContentDatabase db;
    content::LoadReport rep;
    content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep);
    return db;
}
}  // namespace

TEST_CASE("story: the shipped serial is 7 town beats + the Jester, all filled", "[story]") {
    const content::ContentDatabase db = loadContent();
    CHECK(db.storyCount() == 8);  // towns 1..7 + the castle Jester

    std::set<int> towns;
    for (const content::StoryBeat& b : db.story()) {
        INFO("story town " << b.town);
        CHECK(b.town >= 1);
        CHECK(b.town <= kCastleTown);
        CHECK(towns.insert(b.town).second);  // unique town
        CHECK(!b.speaker.empty());
        CHECK(!b.title.empty());
        CHECK(!b.body.empty());
    }
    // Every town installment plus the Jester beat resolves.
    for (int t = 1; t <= kStoryTownCount; ++t) {
        CHECK(db.findStoryBeat(t) != nullptr);
    }
    const content::StoryBeat* jester = db.findStoryBeat(kCastleTown);
    REQUIRE(jester != nullptr);
    CHECK(jester->speaker == "The Jester");
    CHECK(db.findStoryBeat(99) == nullptr);  // no beat for a non-existent place
}

TEST_CASE("story: every beat fits the dialog panel", "[story][lint]") {
    // Conservative measure (7px/char at size 10 overestimates the real font), so a
    // pass here means the real StoryDialogState render fits too.
    const cd::ui::TextMeasure measure = [](const std::string& text, int fontSize) {
        return static_cast<int>(text.size()) * (fontSize * 7) / 10;
    };
    constexpr int kTextW = 320 - 16;  // StoryDialogState panel minus padding
    constexpr int kMaxLines = 12;     // keeps the tallest panel well inside 240
    const content::ContentDatabase db = loadContent();
    for (const content::StoryBeat& b : db.story()) {
        INFO("story town " << b.town);
        const auto lines = cd::ui::wrapText(b.body, kTextW, 10, measure);
        CHECK(lines.size() >= 1);
        CHECK(lines.size() <= kMaxLines);
        for (const std::string& line : lines) {
            CHECK(measure(line, 10) <= kTextW);
        }
    }
}

TEST_CASE("story: the read-progress mask gates the Jester on all seven", "[story]") {
    CHECK(kStoryTownCount == 7);
    int met = 0;
    CHECK_FALSE(storyAllHeard(met));
    for (int t = 1; t <= 7; ++t) {
        CHECK_FALSE(storyHeard(met, t));
        met |= storyBit(t);
        CHECK(storyHeard(met, t));
        CHECK(storyAllHeard(met) == (t == 7));  // only after the seventh
    }
    CHECK(met == kStoryAllMask);
    // The castle's own "town" is not part of the unlock mask.
    CHECK(storyBit(kCastleTown) == 0);
    CHECK(storyBit(0) == 0);
}
