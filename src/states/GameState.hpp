#pragma once

// Base class for every screen/mode. States live on a StateStack and may be
// transparent (let the state below update and/or render).
//
// This header is intentionally raylib-free so StateStack logic can be unit
// tested with lightweight fake states.

namespace cd {

class StateStack;
class Input;

class GameState {
public:
    explicit GameState(StateStack& stack) : stack_(&stack) {}
    virtual ~GameState() = default;

    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    // Lifecycle.
    virtual void onEnter() {}    // pushed onto the stack
    virtual void onExit() {}     // about to be removed
    virtual void onPause() {}    // another state was pushed above this one
    virtual void onResume() {}   // the state above this one was popped

    // Per-frame. Only the TOP state receives input (states are modal).
    virtual void handleInput(const Input& input) { (void)input; }
    virtual void update(float dt) { (void)dt; }
    virtual void render() {}

    // Transparency: should the state directly below also update / render?
    virtual bool updatesBelow() const { return false; }
    virtual bool rendersBelow() const { return false; }

protected:
    StateStack& stack() { return *stack_; }

private:
    StateStack* stack_;
};

}  // namespace cd
