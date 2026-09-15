// M117 - the Controls page lists the F1 debug-overlay row only where the
// overlay is compiled in (dev builds); a Release build's page never
// advertises a key that does nothing there. The list is a pure accessor so
// both presets pin their own truth.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "states/HelpState.hpp"

using namespace cd;

TEST_CASE("help: the Controls page lists ToggleDebug only with the debug overlay",
          "[help][input][m117]") {
    const std::vector<InputAction>& shown = helpShownActions();
    const bool listsF1 =
        std::find(shown.begin(), shown.end(), InputAction::ToggleDebug) != shown.end();
#ifdef CRYSTAL_DEBUG_OVERLAY
    CHECK(listsF1);
    CHECK(shown.size() == 9);
#else
    CHECK_FALSE(listsF1);
    CHECK(shown.size() == 8);
#endif
    // The everyday rows are always there, in the page's order.
    REQUIRE(shown.size() >= 8);
    CHECK(shown[0] == InputAction::MoveUp);
    CHECK(shown[4] == InputAction::Confirm);
    CHECK(shown[5] == InputAction::Cancel);
    CHECK(shown[7] == InputAction::TextBackspace);
}
