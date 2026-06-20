#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "input/Input.hpp"
#include "states/GameState.hpp"
#include "states/StateStack.hpp"

using namespace cd;

namespace {

struct Record {
    int enter = 0;
    int exit = 0;
    int pause = 0;
    int resume = 0;
    int update = 0;
    int render = 0;
    int input = 0;
};

// Lightweight fake state used to observe StateStack behavior without raylib.
class FakeState : public GameState {
public:
    FakeState(StateStack& stack, Record& rec, bool updatesBelow, bool rendersBelow)
        : GameState(stack), rec_(rec), updatesBelow_(updatesBelow), rendersBelow_(rendersBelow) {}

    void onEnter() override { ++rec_.enter; }
    void onExit() override { ++rec_.exit; }
    void onPause() override { ++rec_.pause; }
    void onResume() override { ++rec_.resume; }
    void handleInput(const Input&) override { ++rec_.input; }
    void update(float) override { ++rec_.update; }
    void render() override { ++rec_.render; }
    bool updatesBelow() const override { return updatesBelow_; }
    bool rendersBelow() const override { return rendersBelow_; }

private:
    Record& rec_;
    bool updatesBelow_;
    bool rendersBelow_;
};

std::unique_ptr<FakeState> make(StateStack& s, Record& r, bool updBelow = false,
                                bool rndBelow = false) {
    return std::make_unique<FakeState>(s, r, updBelow, rndBelow);
}

// Pops itself the first time it is resumed (models DungeonState returning to
// town on game-over from within onResume).
class SelfPopOnResume : public GameState {
public:
    explicit SelfPopOnResume(StateStack& s) : GameState(s) {}
    void onResume() override {
        if (!done_) {
            done_ = true;
            stack().popState();
        }
    }

private:
    bool done_ = false;
};

}  // namespace

TEST_CASE("stack: transitions are queued until applied", "[states]") {
    StateStack stack;
    Record a;
    stack.pushState(make(stack, a));
    REQUIRE(stack.size() == 0);  // not applied yet
    REQUIRE(a.enter == 0);

    stack.applyPending();
    REQUIRE(stack.size() == 1);
    REQUIRE(a.enter == 1);
}

TEST_CASE("stack: pushing pauses the previous top and enters the new one", "[states]") {
    StateStack stack;
    Record a;
    Record b;
    stack.pushState(make(stack, a));
    stack.applyPending();
    stack.pushState(make(stack, b));
    stack.applyPending();

    REQUIRE(stack.size() == 2);
    REQUIRE(a.pause == 1);
    REQUIRE(a.resume == 0);
    REQUIRE(b.enter == 1);
}

TEST_CASE("stack: popping exits top and resumes the one below", "[states]") {
    StateStack stack;
    Record a;
    Record b;
    stack.pushState(make(stack, a));
    stack.applyPending();
    stack.pushState(make(stack, b));
    stack.applyPending();

    stack.popState();
    stack.applyPending();

    REQUIRE(stack.size() == 1);
    REQUIRE(b.exit == 1);
    REQUIRE(a.resume == 1);
}

TEST_CASE("stack: replace swaps the top in place", "[states]") {
    StateStack stack;
    Record a;
    Record b;
    stack.pushState(make(stack, a));
    stack.applyPending();
    stack.replaceState(make(stack, b));
    stack.applyPending();

    REQUIRE(stack.size() == 1);
    REQUIRE(a.exit == 1);
    REQUIRE(b.enter == 1);
}

TEST_CASE("stack: clear exits every state", "[states]") {
    StateStack stack;
    Record a;
    Record b;
    stack.pushState(make(stack, a));
    stack.applyPending();
    stack.pushState(make(stack, b));
    stack.applyPending();

    stack.clearStates();
    stack.applyPending();

    REQUIRE(stack.empty());
    REQUIRE(a.exit == 1);
    REQUIRE(b.exit == 1);
}

TEST_CASE("stack: opaque top updates only itself; transparent top updates below",
          "[states]") {
    SECTION("opaque overlay") {
        StateStack stack;
        Record bottom;
        Record top;
        stack.pushState(make(stack, bottom));
        stack.applyPending();
        stack.pushState(make(stack, top, /*updatesBelow=*/false));
        stack.applyPending();

        stack.update(1.0f / 60.0f);
        REQUIRE(top.update == 1);
        REQUIRE(bottom.update == 0);
    }
    SECTION("transparent overlay") {
        StateStack stack;
        Record bottom;
        Record top;
        stack.pushState(make(stack, bottom));
        stack.applyPending();
        stack.pushState(make(stack, top, /*updatesBelow=*/true));
        stack.applyPending();

        stack.update(1.0f / 60.0f);
        REQUIRE(top.update == 1);
        REQUIRE(bottom.update == 1);
    }
}

TEST_CASE("stack: rendering respects transparency", "[states]") {
    StateStack stack;
    Record bottom;
    Record top;
    stack.pushState(make(stack, bottom));
    stack.applyPending();
    stack.pushState(make(stack, top, /*updatesBelow=*/false, /*rendersBelow=*/true));
    stack.applyPending();

    stack.render();
    REQUIRE(top.render == 1);
    REQUIRE(bottom.render == 1);  // visible through the transparent overlay
}

TEST_CASE("stack: only the top state receives input", "[states]") {
    StateStack stack;
    Record bottom;
    Record top;
    stack.pushState(make(stack, bottom));
    stack.applyPending();
    stack.pushState(make(stack, top));
    stack.applyPending();

    Input input;  // default query: every action reads as inactive
    stack.handleInput(input);
    REQUIRE(top.input == 1);
    REQUIRE(bottom.input == 0);
}

TEST_CASE("stack: a transition queued from a lifecycle hook is applied", "[states]") {
    StateStack stack;
    Record base;
    Record top;
    stack.pushState(make(stack, base));
    stack.applyPending();
    stack.pushState(std::make_unique<SelfPopOnResume>(stack));
    stack.applyPending();
    stack.pushState(make(stack, top));
    stack.applyPending();
    REQUIRE(stack.size() == 3);

    // Popping the top resumes the middle state, whose onResume pops itself.
    // That follow-up pop must be applied, not dropped.
    stack.popState();
    stack.applyPending();
    REQUIRE(stack.size() == 1);  // only the base remains
}

TEST_CASE("stack: update applies pending changes first", "[states]") {
    StateStack stack;
    Record a;
    stack.pushState(make(stack, a));  // queued only
    stack.update(1.0f / 60.0f);       // should apply, then update
    REQUIRE(stack.size() == 1);
    REQUIRE(a.enter == 1);
    REQUIRE(a.update == 1);
}
