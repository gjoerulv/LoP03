#include "input/Remap.hpp"

#include <vector>

namespace cd::input {

namespace {

// Rebind `action`'s list on one device to {newCode}, swapping with a
// conflicting remappable owner when possible. Device is abstracted through
// the two accessors so key and button remapping share one implementation.
template <typename GetList, typename SetList>
RemapResult remapOnDevice(InputAction action, int newCode, const GetList& list,
                          const SetList& setList) {
    if (!isRemappable(action)) {
        return {RemapOutcome::Blocked, action};
    }

    // Find a conflicting owner among the other remappable actions.
    InputAction owner = action;
    bool hasOwner = false;
    for (InputAction other : kRemappableActions) {
        if (other == action) {
            continue;
        }
        for (int code : list(other)) {
            if (code == newCode) {
                owner = other;
                hasOwner = true;
                break;
            }
        }
        if (hasOwner) {
            break;
        }
    }

    const std::vector<int> oldBindings = list(action);

    if (!hasOwner) {
        setList(action, {newCode});
        return {RemapOutcome::Rebound, action};
    }

    // Swap: the previous owner loses newCode; it must end up non-empty, so it
    // takes this action's old primary binding. Without a donor, block.
    std::vector<int> ownerBindings;
    for (int code : list(owner)) {
        if (code != newCode) {
            ownerBindings.push_back(code);
        }
    }
    if (ownerBindings.empty()) {
        if (oldBindings.empty()) {
            return {RemapOutcome::Blocked, owner};
        }
        ownerBindings.push_back(oldBindings.front());
    }
    setList(owner, ownerBindings);
    setList(action, {newCode});
    return {RemapOutcome::Swapped, owner};
}

}  // namespace

bool isRemappable(InputAction action) {
    for (InputAction a : kRemappableActions) {
        if (a == action) {
            return true;
        }
    }
    return false;
}

RemapResult remapKey(InputMap& map, InputAction action, int newKey) {
    if (newKey == kReservedKeyEscape) {
        return {RemapOutcome::Blocked, action};
    }
    return remapOnDevice(
        action, newKey, [&map](InputAction a) -> const std::vector<int>& { return map.keys(a); },
        [&map](InputAction a, const std::vector<int>& codes) {
            const std::vector<int> buttons = map.buttons(a);
            map.clear(a);
            for (int code : codes) {
                map.bindKey(a, code);
            }
            for (int btn : buttons) {
                map.bindButton(a, btn);
            }
        });
}

RemapResult remapButton(InputMap& map, InputAction action, int newButton) {
    return remapOnDevice(
        action, newButton,
        [&map](InputAction a) -> const std::vector<int>& { return map.buttons(a); },
        [&map](InputAction a, const std::vector<int>& codes) {
            const std::vector<int> keys = map.keys(a);
            map.clear(a);
            for (int key : keys) {
                map.bindKey(a, key);
            }
            for (int code : codes) {
                map.bindButton(a, code);
            }
        });
}

void resetBindings(InputMap& map) { map = InputMap{}; }

SlotResult assignKeySlot(InputMap& map, InputAction action, int slot, int key,
                         bool confirmSteal) {
    if (!isRemappable(action) || key == kReservedKeyEscape || slot < 0 ||
        slot >= kKeySlotCount) {
        return {SlotOutcome::Blocked, action, 0};
    }

    // Where does the key live today (among the remappable keyboard lists)?
    InputAction owner = action;
    int ownerSlot = -1;
    bool foreign = false;
    for (InputAction other : kRemappableActions) {
        const auto& keys = map.keys(other);
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] == key) {
                owner = other;
                ownerSlot = static_cast<int>(i);
                foreign = other != action;
            }
        }
    }
    if (foreign) {
        if (!confirmSteal) {
            return {SlotOutcome::NeedsConfirm, owner, ownerSlot};
        }
        if (map.keys(owner).size() <= 1) {
            return {SlotOutcome::Blocked, owner, ownerSlot};  // never strand
        }
    }

    const auto setKeys = [&map](InputAction a, const std::vector<int>& keys) {
        const std::vector<int> buttons = map.buttons(a);
        map.clear(a);
        for (int k : keys) {
            map.bindKey(a, k);
        }
        for (int b : buttons) {
            map.bindButton(a, b);
        }
    };

    // Pull the key out of wherever it lives — the old owner, or this action's
    // other slot (a move between own slots).
    if (ownerSlot >= 0) {
        std::vector<int> ownerKeys = map.keys(owner);
        ownerKeys.erase(ownerKeys.begin() + ownerSlot);
        setKeys(owner, ownerKeys);
    }

    std::vector<int> keys = map.keys(action);
    const std::size_t target = std::min(static_cast<std::size_t>(slot), keys.size());
    if (target < keys.size()) {
        keys[target] = key;  // a direct slot remap replaces that slot's key
    } else {
        keys.push_back(key);  // pack left: no gaps between slots
    }
    setKeys(action, keys);
    return {foreign ? SlotOutcome::Stolen : SlotOutcome::Rebound, owner,
            ownerSlot < 0 ? 0 : ownerSlot};
}

}  // namespace cd::input
