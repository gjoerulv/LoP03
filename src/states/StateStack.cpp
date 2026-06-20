#include "states/StateStack.hpp"

#include <utility>

namespace cd {

void StateStack::pushState(std::unique_ptr<GameState> state) {
    pending_.push_back({ChangeKind::Push, std::move(state)});
}

void StateStack::popState() { pending_.push_back({ChangeKind::Pop, nullptr}); }

void StateStack::clearStates() { pending_.push_back({ChangeKind::Clear, nullptr}); }

void StateStack::replaceState(std::unique_ptr<GameState> state) {
    pending_.push_back({ChangeKind::Pop, nullptr});
    pending_.push_back({ChangeKind::Push, std::move(state)});
}

void StateStack::applyPending() {
    // Lifecycle hooks (onEnter/onExit/onResume/onPause) may themselves request
    // transitions. Process in batches: swap the queue out, apply it, and let any
    // hook-queued changes accumulate in pending_ for the next batch. This avoids
    // mutating the container we're iterating and ensures those follow-up
    // transitions are applied rather than silently dropped.
    while (!pending_.empty()) {
        std::vector<PendingChange> batch;
        batch.swap(pending_);
        for (auto& change : batch) {
            switch (change.kind) {
                case ChangeKind::Push: {
                    if (!states_.empty()) {
                        states_.back()->onPause();
                    }
                    states_.push_back(std::move(change.state));
                    states_.back()->onEnter();
                    break;
                }
                case ChangeKind::Pop: {
                    if (!states_.empty()) {
                        states_.back()->onExit();
                        states_.pop_back();
                        if (!states_.empty()) {
                            states_.back()->onResume();
                        }
                    }
                    break;
                }
                case ChangeKind::Clear: {
                    while (!states_.empty()) {
                        states_.back()->onExit();
                        states_.pop_back();
                    }
                    break;
                }
            }
        }
    }
}

std::size_t StateStack::lowestVisibleForUpdate() const {
    std::size_t start = states_.size() - 1;
    while (start > 0 && states_[start]->updatesBelow()) {
        --start;
    }
    return start;
}

std::size_t StateStack::lowestVisibleForRender() const {
    std::size_t start = states_.size() - 1;
    while (start > 0 && states_[start]->rendersBelow()) {
        --start;
    }
    return start;
}

void StateStack::handleInput(const Input& input) {
    applyPending();
    if (!states_.empty()) {
        states_.back()->handleInput(input);  // only the top state is interactive
    }
}

void StateStack::update(float dt) {
    applyPending();
    if (states_.empty()) {
        return;
    }
    const std::size_t start = lowestVisibleForUpdate();
    for (std::size_t i = start; i < states_.size(); ++i) {
        states_[i]->update(dt);
    }
}

void StateStack::render() {
    if (states_.empty()) {
        return;
    }
    const std::size_t start = lowestVisibleForRender();
    for (std::size_t i = start; i < states_.size(); ++i) {
        states_[i]->render();  // bottom-up, so the top state draws last
    }
}

}  // namespace cd
