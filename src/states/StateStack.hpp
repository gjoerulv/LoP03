#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "states/GameState.hpp"

namespace cd {

class Input;

// Owns a stack of GameStates. Transitions are QUEUED and applied between frames
// (applyPending), so a state may request push/pop/clear from inside its own
// update()/handleInput() without invalidating the stack mid-iteration.
class StateStack {
public:
    void pushState(std::unique_ptr<GameState> state);
    void popState();
    void clearStates();
    // Convenience: pop the current top and push a replacement.
    void replaceState(std::unique_ptr<GameState> state);

    // Applies any queued transitions, firing the appropriate lifecycle hooks.
    void applyPending();

    void handleInput(const Input& input);
    void update(float dt);
    void render();

    bool empty() const { return states_.empty(); }
    std::size_t size() const { return states_.size(); }
    GameState* top() { return states_.empty() ? nullptr : states_.back().get(); }

private:
    enum class ChangeKind { Push, Pop, Clear };
    struct PendingChange {
        ChangeKind kind;
        std::unique_ptr<GameState> state;  // only for Push
    };

    // Index of the lowest state that participates this frame, walking down from
    // the top while the state above is transparent for the given query.
    std::size_t lowestVisibleForUpdate() const;
    std::size_t lowestVisibleForRender() const;

    std::vector<std::unique_ptr<GameState>> states_;
    std::vector<PendingChange> pending_;
};

}  // namespace cd
