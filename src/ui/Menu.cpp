#include "ui/Menu.hpp"

#include <utility>

namespace cd::ui {

Menu::Menu(std::vector<MenuItem> items) : items_(std::move(items)) { setCursor(0); }

void Menu::setItems(std::vector<MenuItem> items) {
    items_ = std::move(items);
    setCursor(0);
}

void Menu::addItem(std::string label, bool enabled) {
    items_.push_back({std::move(label), enabled});
}

void Menu::clear() {
    items_.clear();
    cursor_ = 0;
}

void Menu::setCursor(int index) {
    if (items_.empty()) {
        cursor_ = 0;
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= static_cast<int>(items_.size())) {
        index = static_cast<int>(items_.size()) - 1;
    }
    cursor_ = index;
    if (!items_[static_cast<std::size_t>(cursor_)].enabled) {
        step(+1);  // settle on the nearest enabled item if possible
    }
}

void Menu::step(int direction) {
    if (items_.empty()) {
        return;
    }
    const int count = static_cast<int>(items_.size());
    for (int i = 0; i < count; ++i) {
        const int next = ((cursor_ + direction * (i + 1)) % count + count) % count;
        if (items_[static_cast<std::size_t>(next)].enabled) {
            cursor_ = next;
            return;
        }
    }
    // No enabled item: leave the cursor where it is.
}

void Menu::moveUp() { step(-1); }
void Menu::moveDown() { step(+1); }

const MenuItem* Menu::current() const {
    if (items_.empty()) {
        return nullptr;
    }
    return &items_[static_cast<std::size_t>(cursor_)];
}

bool Menu::currentEnabled() const {
    const MenuItem* item = current();
    return item != nullptr && item->enabled;
}

}  // namespace cd::ui
