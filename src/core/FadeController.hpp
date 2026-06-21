#pragma once

#include <algorithm>

// Drives a fade-IN from black: start() makes the screen fully black, and it
// reveals over `duration`. The renderer draws a black overlay at coverAlpha().
// Pure (no raylib), so the timing logic is unit-testable.

namespace cd {

class FadeController {
public:
    void start(float duration = 0.35f) {
        duration_ = duration > 0.01f ? duration : 0.01f;
        timer_ = duration_;
    }

    void update(float dt) { timer_ = std::max(0.0f, timer_ - dt); }

    // 1.0 = fully black, 0.0 = fully revealed.
    float coverAlpha() const { return duration_ > 0.0f ? timer_ / duration_ : 0.0f; }
    bool active() const { return timer_ > 0.0f; }

private:
    float timer_ = 0.0f;
    float duration_ = 0.35f;
};

}  // namespace cd
