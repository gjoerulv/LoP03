#pragma once

#include <string>

#include "battle/Battle.hpp"
#include "states/GameState.hpp"

// M65: the puzzle-map treasure dig. One guarded fight — a seeded
// dungeon-roster boss (with its authored court) at the scale of the dungeon
// that yielded the fourth piece — with castle semantics: no dungeon score,
// no gold penalty, the 1-HP defeat clamp, retry allowed (the reveal stays
// until the treasure is won). Victory digs up the treasure: the next unawarded
// Lost Scroll, learned IMMEDIATELY by a picked member (owner rule), or a
// legendary token + gold once the pool is spent. The reveal then clears and
// the puzzle cycle restarts.

namespace cd {

struct AppContext;
class StateStack;

class TreasureFightState : public GameState {
public:
    TreasureFightState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void onResume() override;
    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    void captureResult();  // deterministic: the won overlay + member picker
#endif

private:
    void finish(bool won);
    void awardTreasure(int memberIndex);  // learn the scroll / pay the fallback

    AppContext& context_;
    battle::BattleResult result_{};
    bool done_ = false;          // result overlay is up
    bool pickingMember_ = false; // a scroll waits for its student
    std::string pendingScrollId_;
    int cursor_ = 0;             // member picker
    std::string resultText_;
};

}  // namespace cd
