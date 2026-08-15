#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "core/SeedParse.hpp"
#include "ui/TextInput.hpp"

using namespace cd::ui;

TEST_CASE("text input: accepts allowed chars, rejects others, respects max length", "[ui]") {
    TextInput t(5);
    t.appendCodepoint('A');
    t.appendCodepoint('!');  // disallowed
    t.appendCodepoint('b');
    t.appendCodepoint('1');
    t.appendCodepoint('-');
    t.appendCodepoint(' ');  // now "Ab1- " -> length 5
    REQUIRE(t.value() == "Ab1- ");
    REQUIRE(t.full());
    t.appendCodepoint('x');  // full, ignored
    REQUIRE(t.value().size() == 5);
}

TEST_CASE("text input: backspace and clear", "[ui]") {
    TextInput t(8, "Hero");
    t.backspace();
    REQUIRE(t.value() == "Her");
    t.clear();
    REQUIRE(t.empty());
    t.backspace();  // must not crash on empty
    REQUIRE(t.empty());
}

TEST_CASE("text input: setValue filters and truncates", "[ui]") {
    TextInput truncated(3, "abcdef");
    REQUIRE(truncated.value() == "abc");

    TextInput filtered(10, "a@b#c");
    REQUIRE(filtered.value() == "abc");
}

TEST_CASE("text input: trimmed removes surrounding spaces", "[ui]") {
    TextInput t(12, "  Rolan  ");
    REQUIRE(t.value() == "  Rolan  ");
    REQUIRE(t.trimmed() == "Rolan");

    TextInput blank(4, "    ");
    REQUIRE(blank.trimmed().empty());
}

TEST_CASE("text input: the Digits filter admits digits alone (M88)", "[ui]") {
    TextInput t(20, "", TextFilter::Digits);
    for (int c : {'0', '5', '9'}) {
        t.appendCodepoint(c);
    }
    for (int c : {'a', 'Z', ' ', '-', '\'', '!', '.'}) {
        t.appendCodepoint(c);  // all refused
    }
    REQUIRE(t.value() == "059");

    // setValue applies the same filter — a pasted/prefilled mixed string keeps
    // only its digits.
    TextInput pre(20, "12ab34", TextFilter::Digits);
    REQUIRE(pre.value() == "1234");
}

TEST_CASE("seed parse: empty and zero keep the old seed; overflow clamps (M88)", "[ui]") {
    CHECK(cd::parseSeedDigits("") == 0);       // empty -> caller keeps the old seed
    CHECK(cd::parseSeedDigits("0") == 0);      // zero is reserved the same way
    CHECK(cd::parseSeedDigits("1") == 1ull);
    CHECK(cd::parseSeedDigits("15113529870800074004") == 15113529870800074004ull);
    CHECK(cd::parseSeedDigits("18446744073709551615") == UINT64_MAX);  // exactly max
    CHECK(cd::parseSeedDigits("18446744073709551616") == UINT64_MAX);  // one past: clamps
    CHECK(cd::parseSeedDigits("99999999999999999999") == UINT64_MAX);  // 20 nines: clamps
    CHECK(cd::parseSeedDigits("007") == 7ull);  // leading zeros are harmless
}
