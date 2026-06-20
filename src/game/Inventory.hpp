#pragma once

#include <string>
#include <vector>

// A simple shared party inventory: item id -> count, kept in insertion order for
// deterministic display and saving. Pure data + small ops, unit-tested.

namespace cd {

struct ItemStack {
    std::string itemId;
    int count = 0;
};

struct Inventory {
    std::vector<ItemStack> stacks;

    void add(const std::string& itemId, int count = 1);
    bool remove(const std::string& itemId, int count = 1);  // false if not enough
    int count(const std::string& itemId) const;
    bool empty() const { return stacks.empty(); }
};

}  // namespace cd
