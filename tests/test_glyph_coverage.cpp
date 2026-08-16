// M87 — glyph coverage: the three-way contract between src/ui/GlyphCoverage.hpp
// (the authority), the generated .fnt descriptors (the shipped font), and the
// shipped JSON content (what the font must be able to render). A future
// translation that uses an unsupported character fails here instead of
// rendering as the '?' fallback glyph in play.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ui/GlyphCoverage.hpp"

using namespace cd::ui;
namespace fs = std::filesystem;

namespace {

// Every codepoint the coverage contract promises a glyph for.
std::vector<std::uint32_t> requiredCodepoints() {
    std::vector<std::uint32_t> out;
    for (std::uint32_t cp = 32; cp <= 0xFF; ++cp) {
        if (glyphs::isSupportedCodepoint(cp)) {
            out.push_back(cp);
        }
    }
    return out;
}

// Char ids declared by a BMFont text descriptor.
std::set<std::uint32_t> fntCharIds(const fs::path& file) {
    std::set<std::uint32_t> ids;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        constexpr const char* kPrefix = "char id=";
        if (line.rfind(kPrefix, 0) == 0) {
            ids.insert(static_cast<std::uint32_t>(
                std::stoul(line.substr(std::string(kPrefix).size()))));
        }
    }
    return ids;
}

}  // namespace

TEST_CASE("glyphs: the supported set is exactly ASCII + the Latin-1 letters + marks",
          "[glyphs]") {
    // Printable ASCII.
    for (std::uint32_t cp = 32; cp <= 126; ++cp) {
        CHECK(glyphs::isSupportedCodepoint(cp));
    }
    // Spot checks across the Latin set the translations need.
    for (const char* s : {"æ", "ø", "å", "Æ", "Ø", "Å",
                          "ä", "ö", "ü", "ß", "é", "è",
                          "ê", "ë", "ç", "ñ", "¡", "¿",
                          "«", "»"}) {
        const std::string text(s);
        std::size_t pos = 0;
        const std::uint32_t cp = glyphs::decodeUtf8(text, pos);
        INFO(text);
        CHECK(glyphs::isSupportedCodepoint(cp));
    }
    // Deliberately out: controls, the Latin-1 math signs, Latin Extended, CJK.
    CHECK_FALSE(glyphs::isSupportedCodepoint('\t'));
    CHECK_FALSE(glyphs::isSupportedCodepoint('\n'));
    CHECK_FALSE(glyphs::isSupportedCodepoint(0xD7));    // multiplication sign
    CHECK_FALSE(glyphs::isSupportedCodepoint(0xF7));    // division sign
    CHECK_FALSE(glyphs::isSupportedCodepoint(0x0153));  // oe ligature (Latin Ext-A)
    CHECK_FALSE(glyphs::isSupportedCodepoint(0x4E00));  // CJK
    // '\n' is allowed IN CONTENT (paragraph semantics), tabs are not.
    CHECK(glyphs::isAllowedInContent('\n'));
    CHECK_FALSE(glyphs::isAllowedInContent('\t'));
    CHECK(requiredCodepoints().size() == 161);  // 95 ASCII + 62 letters + 4 marks
}

TEST_CASE("glyphs: decodeUtf8 handles ascii, multibyte, and malformed input",
          "[glyphs]") {
    std::string text = "a\xC3\xA9z";
    std::size_t pos = 0;
    CHECK(glyphs::decodeUtf8(text, pos) == 'a');
    CHECK(glyphs::decodeUtf8(text, pos) == 0xE9);
    CHECK(glyphs::decodeUtf8(text, pos) == 'z');
    CHECK(pos == text.size());

    std::string stray = "\xA9";  // continuation byte with no lead
    pos = 0;
    CHECK(glyphs::decodeUtf8(stray, pos) == glyphs::kInvalidUtf8);
    CHECK(pos == 1);  // always advances, so scans terminate

    std::string truncated = "\xC3";  // lead byte, missing continuation
    pos = 0;
    CHECK(glyphs::decodeUtf8(truncated, pos) == glyphs::kInvalidUtf8);
}

TEST_CASE("glyphs: firstUnsupportedCodepoint finds the offender or returns 0",
          "[glyphs]") {
    CHECK(glyphs::firstUnsupportedCodepoint("Plain ASCII, with lines.\nNew paragraph.") == 0);
    CHECK(glyphs::firstUnsupportedCodepoint("V\xC3\xA6pnet gj\xC3\xB8k p\xC3\xA5 \xC3\xB8y") ==
          0);  // Norwegian
    CHECK(glyphs::firstUnsupportedCodepoint("tab\there") ==
          static_cast<std::uint32_t>('\t'));
    CHECK(glyphs::firstUnsupportedCodepoint("bad\xFFtail") == glyphs::kInvalidUtf8);
    // U+0153 (oe ligature) is outside the supported set - it must be caught.
    CHECK(glyphs::firstUnsupportedCodepoint("c\xC5\x93ur") == 0x0153);
}

#ifdef CRYSTAL_TEST_ASSETS_DIR
TEST_CASE("glyphs: every shipped .fnt covers the whole contract", "[glyphs]") {
    const fs::path fonts = fs::path(CRYSTAL_TEST_ASSETS_DIR) / "fonts";
    const std::vector<std::uint32_t> required = requiredCodepoints();
    for (const char* name : {"font_small.fnt", "font_main.fnt", "font_title.fnt"}) {
        const fs::path file = fonts / name;
        INFO(file.string());
        REQUIRE(fs::exists(file));
        const std::set<std::uint32_t> ids = fntCharIds(file);
        for (const std::uint32_t cp : required) {
            INFO("codepoint " << cp);
            CHECK(ids.count(cp) == 1);
        }
    }
}
#endif

#ifdef CRYSTAL_TEST_DATA_DIR
TEST_CASE("glyphs: all shipped content text is renderable by the font", "[glyphs]") {
    // Walks every string in every shipped JSON (names, descriptions, story,
    // flavor, lore, ids alike): valid UTF-8, only supported glyphs plus
    // semantic newlines, no tabs/control characters. This is the tripwire a
    // localized content drop runs into BEFORE anything renders as '?'.
    int files = 0;
    long strings = 0;
    const std::function<void(const nlohmann::json&, const std::string&)> walk =
        [&](const nlohmann::json& node, const std::string& where) {
            if (node.is_string()) {
                ++strings;
                const std::string value = node.get<std::string>();
                const std::uint32_t bad = glyphs::firstUnsupportedCodepoint(value);
                INFO(where << ": '" << value << "' codepoint " << bad);
                CHECK(bad == 0);
            } else if (node.is_object()) {
                for (const auto& [key, child] : node.items()) {
                    CHECK(glyphs::firstUnsupportedCodepoint(key) == 0);
                    walk(child, where + "/" + key);
                }
            } else if (node.is_array()) {
                for (const nlohmann::json& child : node) {
                    walk(child, where + "[]");
                }
            }
        };
    for (const fs::directory_entry& e : fs::directory_iterator(CRYSTAL_TEST_DATA_DIR)) {
        if (!e.is_regular_file() || e.path().extension() != ".json") {
            continue;
        }
        ++files;
        std::ifstream in(e.path(), std::ios::binary);
        const nlohmann::json doc = nlohmann::json::parse(in, nullptr, false);
        REQUIRE_FALSE(doc.is_discarded());
        walk(doc, e.path().filename().string());
    }
    CHECK(files >= 12);      // the full shipped content tree was really swept
    CHECK(strings > 1000);   // and it actually contained the game's text
}
#endif
