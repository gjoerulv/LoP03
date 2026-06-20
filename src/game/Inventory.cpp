#include "game/Inventory.hpp"

namespace cd {

void Inventory::add(const std::string& itemId, int count) {
    if (count <= 0) {
        return;
    }
    for (ItemStack& s : stacks) {
        if (s.itemId == itemId) {
            s.count += count;
            return;
        }
    }
    stacks.push_back({itemId, count});
}

bool Inventory::remove(const std::string& itemId, int count) {
    if (count <= 0) {
        return true;
    }
    for (std::size_t i = 0; i < stacks.size(); ++i) {
        if (stacks[i].itemId == itemId) {
            if (stacks[i].count < count) {
                return false;
            }
            stacks[i].count -= count;
            if (stacks[i].count == 0) {
                stacks.erase(stacks.begin() + static_cast<std::ptrdiff_t>(i));
            }
            return true;
        }
    }
    return false;
}

int Inventory::count(const std::string& itemId) const {
    for (const ItemStack& s : stacks) {
        if (s.itemId == itemId) {
            return s.count;
        }
    }
    return 0;
}

}  // namespace cd
