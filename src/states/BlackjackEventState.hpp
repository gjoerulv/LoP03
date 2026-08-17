#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dungeon/DungeonModel.hpp"
#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// M104 (owner event 3): one seeded hand of blackjack over the dimmed dungeon.
// The BET was already taken by the caller; a win here pays it back doubled, a
// push returns it, a loss keeps it. Cards come from the pure gamble::* stream
// (hash of dungeon seed / room / draw index), so a reload replays the same
// shoe — save-scumming buys nothing. Confirm hits, Cancel stands; the hand
// always finishes (the walk-away moment was the bet modal before this).
// `ev` may be null (capture scenes); when set, the hand's end resolves it.
class BlackjackEventState : public GameState {
public:
    BlackjackEventState(StateStack& stack, AppContext& context, int bet, std::uint64_t seed,
                        int room, dungeon::RoomEvent* ev);

    void handleInput(const Input& input) override;
    void render() override;
    bool rendersBelow() const override { return true; }

private:
    int draw();           // next card off the seeded shoe
    void finishHand();    // dealer plays to 17; payout + resolution

    AppContext& context_;
    int bet_;
    std::uint64_t seed_;
    int room_;
    dungeon::RoomEvent* ev_;
    int drawIndex_ = 0;
    std::vector<int> player_;
    std::vector<int> dealer_;
    bool done_ = false;
    std::string resultText_;
};

}  // namespace cd
