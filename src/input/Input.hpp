#pragma once

#include <utility>

#include "input/InputMap.hpp"

// Per-frame input facade handed to states. Holds the bindings and the current
// frame's query; states ask in action terms: input.pressed(InputAction::Confirm).

namespace cd {

class Input {
public:
    void setQuery(InputQuery query) { query_ = std::move(query); }

    InputMap& map() { return map_; }
    const InputMap& map() const { return map_; }

    bool down(InputAction a) const { return map_.isDown(a, query_); }
    bool pressed(InputAction a) const { return map_.isPressed(a, query_); }
    bool released(InputAction a) const { return map_.isReleased(a, query_); }

private:
    InputMap map_;
    InputQuery query_;
};

}  // namespace cd
