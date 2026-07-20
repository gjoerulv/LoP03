#include <catch2/catch_test_macros.hpp>

#include "ui/ScrollWindow.hpp"

using cd::ui::ScrollWindow;

TEST_CASE("scroll: short lists never scroll") {
    ScrollWindow w;
    w.follow(3, 5, 2);
    CHECK(w.top() == 0);
    CHECK(w.visibleCount(3, 5) == 3);
    CHECK_FALSE(w.moreAbove());
    CHECK_FALSE(w.moreBelow(3, 5));
}

TEST_CASE("scroll: cursor moving below the window scrolls down") {
    ScrollWindow w;
    w.follow(10, 3, 0);
    CHECK(w.top() == 0);
    w.follow(10, 3, 3);  // just past the last visible row (0..2)
    CHECK(w.top() == 1);
    w.follow(10, 3, 9);
    CHECK(w.top() == 7);
    CHECK(w.moreAbove());
    CHECK_FALSE(w.moreBelow(10, 3));
}

TEST_CASE("scroll: cursor moving above the window scrolls up") {
    ScrollWindow w;
    w.follow(10, 3, 9);
    w.follow(10, 3, 6);  // still visible (7..9? no: 7,8,9 -> 6 is above)
    CHECK(w.top() == 6);
    w.follow(10, 3, 0);  // wrap to the top
    CHECK(w.top() == 0);
    CHECK_FALSE(w.moreAbove());
    CHECK(w.moreBelow(10, 3));
}

TEST_CASE("scroll: cursor inside the window does not move it") {
    ScrollWindow w;
    w.follow(10, 3, 4);  // top becomes 2 (4-3+1)
    const int top = w.top();
    w.follow(10, 3, 3);
    CHECK(w.top() == top);
    w.follow(10, 3, 2);
    CHECK(w.top() == top);
}

TEST_CASE("scroll: wrap-around from bottom to top snaps the window") {
    ScrollWindow w;
    w.follow(25, 12, 24);
    CHECK(w.top() == 13);
    w.follow(25, 12, 0);  // menu wraps
    CHECK(w.top() == 0);
}

TEST_CASE("scroll: free scrolling clamps at both ends") {
    ScrollWindow w;
    w.scrollBy(14, 12, -1);
    CHECK(w.top() == 0);
    w.scrollBy(14, 12, 1);
    CHECK(w.top() == 1);
    w.scrollBy(14, 12, 100);
    CHECK(w.top() == 2);  // 14 items, 12 visible -> max top 2
    CHECK(w.moreAbove());
    CHECK_FALSE(w.moreBelow(14, 12));
}

TEST_CASE("scroll: shrinking list keeps the window valid") {
    ScrollWindow w;
    w.follow(25, 12, 24);
    w.follow(5, 12, 1);  // list rebuilt smaller
    CHECK(w.top() == 0);
    CHECK(w.visibleCount(5, 12) == 5);
}

TEST_CASE("scroll: empty and single-row edge cases") {
    ScrollWindow w;
    w.follow(0, 3, 0);
    CHECK(w.top() == 0);
    CHECK(w.visibleCount(0, 3) == 0);
    w.follow(1, 1, 0);
    CHECK(w.top() == 0);
    CHECK(w.visibleCount(1, 1) == 1);
}
