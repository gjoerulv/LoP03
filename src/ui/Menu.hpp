#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Pure menu model: a vertical list with a cursor that skips disabled items and
// wraps. No raylib, so navigation logic is unit-tested headlessly. Rendering is
// done separately (ui/UiDraw).

namespace cd::ui {

struct MenuItem {
    std::string label;
    bool enabled = true;
    // Optional trailing column (cost, count, tag). Rendered right-aligned at the
    // row's right edge by drawMenuScrolled, so it is never squeezed out by a long
    // label — the label is fitted to whatever room is left instead.
    std::string suffix;
};

class Menu {
public:
    Menu() = default;
    explicit Menu(std::vector<MenuItem> items);

    void setItems(std::vector<MenuItem> items);
    void addItem(std::string label, bool enabled = true);
    void clear();

    const std::vector<MenuItem>& items() const { return items_; }
    std::size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    int cursor() const { return cursor_; }
    void setCursor(int index);  // clamps into range, then nudges to an enabled item

    void moveUp();    // previous enabled item, wrapping
    void moveDown();  // next enabled item, wrapping

    const MenuItem* current() const;  // nullptr if empty
    bool currentEnabled() const;

private:
    void step(int direction);  // move cursor to the next enabled item in a direction

    std::vector<MenuItem> items_;
    int cursor_ = 0;
};

}  // namespace cd::ui
