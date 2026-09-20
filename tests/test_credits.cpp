// M120 - the Credits & Licenses page: the embedded text is the shipped
// packaging/LICENSES.txt (core/Licenses.hpp, generated at configure time), the
// page's credit line and the file agree, and the pure reflow keeps headings,
// copyright lines and numbered clauses on their own lines while rejoining the
// file's hard-wrapped prose.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/Licenses.hpp"
#include "game/Credits.hpp"
#include "ui/GlyphCoverage.hpp"

using namespace cd;

namespace {
bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}
}  // namespace

TEST_CASE("credits: the embedded text is the shipped license file", "[credits][m120]") {
    const std::string text = licenses::kText;
    REQUIRE_FALSE(text.empty());
    // Every shipped third party is named with its license.
    CHECK(contains(text, "raylib"));
    CHECK(contains(text, "zlib/libpng"));
    CHECK(contains(text, "nlohmann/json"));
    CHECK(contains(text, "MIT License"));
    // The working credit lives in the file too, so the page and the package agree.
    CHECK(contains(text, credits::kCreditLine));
}

TEST_CASE("credits: every embedded character has a glyph", "[credits][m120][glyphs]") {
    const std::string text = licenses::kText;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::uint32_t cp = ui::glyphs::decodeUtf8(text, pos);
        if (cp == '\r') {
            continue;  // a CRLF checkout; the reflow strips it
        }
        INFO("codepoint " << cp << " near byte " << pos);
        CHECK(ui::glyphs::isAllowedInContent(cp));
    }
}

TEST_CASE("credits: the reflow rejoins prose and keeps the structural lines",
          "[credits][m120]") {
    const std::string raw =
        "TITLE OF THE FILE\r\n"
        "=================\r\n"
        "\r\n"
        "A game by SmettPlay.\r\n"
        "\r\n"
        "\r\n"
        "SECTION\r\n"
        "\r\n"
        "First sentence is hard\r\n"
        "wrapped across lines.\r\n"
        "\r\n"
        "lib (example.org)\r\n"
        "  Copyright (c) 2000 Someone\r\n"
        "  Some license. Permission is\r\n"
        "  granted, subject to:\r\n"
        "    1. Clause one runs\r\n"
        "       over two lines.\r\n"
        "    2. Clause two.\r\n";
    const std::string body = credits::bodyFromLicenses(raw);
    CHECK(body ==
          "SECTION\n\n"
          "First sentence is hard wrapped across lines.\n\n"
          "lib (example.org)\n"
          "Copyright (c) 2000 Someone\n"
          "Some license. Permission is granted, subject to:\n"
          "1. Clause one runs over two lines.\n"
          "2. Clause two.");
}

TEST_CASE("credits: the shipped body drops the title and the credit, keeps the rest",
          "[credits][m120]") {
    const std::string body = credits::bodyFromLicenses(licenses::kText);
    CHECK_FALSE(contains(body, "CREDITS AND LICENSES"));   // the header band says it
    CHECK_FALSE(contains(body, credits::kCreditLine));     // the page draws it itself
    CHECK_FALSE(contains(body, "===="));
    CHECK_FALSE(contains(body, "\r"));
    CHECK(body.rfind("GAME CONTENT", 0) == 0);
    CHECK(contains(body, "\nCopyright (c) 2013-2025 Ramon Santamaria (@raysan5)\n"));
    CHECK(contains(body, "\n1. The origin of this software must not be misrepresented; you must "
                         "not claim that you wrote the original software.\n"));
    CHECK(contains(body, "(Catch2 is used for development testing only and does not ship in the "
                         "game executable.)"));
    CHECK_FALSE(contains(body, "\n\n\n"));  // one paragraph break, however many were authored
}
