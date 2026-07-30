#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"

// M71: the victory celebration — shown above the reckoning after a dungeon
// clear with NO stakes penalty, and after beating the King, the Deadly Duck,
// or the Boss Rush (the Endless Rush has no "beating" and is excluded). One
// headline number (the score, or the challenge's turns), the team jumping in
// different rhythms and heights, the run's MVP (most damage dealt, M42) on a
// pedestal, and any KO'd member lying where they fell. Presentation only:
// reads the party, writes nothing, pops on Confirm/Cancel.

namespace cd {

struct AppContext;

class CelebrationState : public GameState {
public:
    // `headline` is the single number line ("Score: 123456" / "Cleared in 18
    // turns!"); `mvpIndex` the party index on the pedestal (-1 = no pedestal).
    CelebrationState(StateStack& stack, AppContext& context, std::string headline, int mvpIndex);

    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

private:
    AppContext& context_;
    std::string headline_;
    std::string punchline_;  // M71: one dry line from kCelebrationPhrases
    int mvp_;
    std::vector<bool> ko_;  // per party member, snapshotted at entry
    float time_ = 0.0f;
};

}  // namespace cd
