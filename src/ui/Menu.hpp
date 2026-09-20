#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Pure menu model: a vertical list with a cursor that skips disabled items and
// wraps. No raylib, so navigation logic is unit-tested headlessly. Rendering is
// done separately (ui/UiDraw).
//
// M121: a list may opt in to FOCUSABLE disabled rows (setFocusDisabled) - the
// cursor then rests on a greyed row like any other, so the player can read
// why it is greyed and open its details; `currentEnabled()` still says no, so
// Confirm refuses it. Skill lists use this; every other menu keeps skipping.

namespace cd::ui {

struct MenuItem {
    std::string label;
    bool enabled = true;
    // Optional trailing column (cost, count, tag). Rendered right-aligned at the
    // row's right edge by drawMenuScrolled, so it is never squeezed out by a long
    // label — the label is fitted to whatever room is left instead.
    std::string suffix;
    // M81: optional gear icon (a manifest texture id, "ui.icon.<category>").
    // Drawn left of the label by drawMenuScrolled when it is given a
    // ResourceManager; once any row in a menu has one, every row indents so
    // the label column stays straight. Pure model — rendering ignores it
    // unless the draw call opts in.
    std::string icon;
    // M121: optional trailing mark (the milestone mark on a skill a held
    // milestone touches), drawn right AFTER the label text. Same opt-in. It
    // costs no column: a row whose label leaves no room simply goes without
    // (the details sheet still shows it) - a mark never clips a name.
    std::string icon2;
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

    // M121: when true the cursor may rest on disabled rows (setCursor stops
    // nudging, moveUp/moveDown stop skipping). Survives setItems/clear - it is
    // a property of the list, not of its current rows. Default false.
    void setFocusDisabled(bool focusable) { focusDisabled_ = focusable; }
    bool focusDisabled() const { return focusDisabled_; }

    void moveUp();    // previous enabled item, wrapping
    void moveDown();  // next enabled item, wrapping

    const MenuItem* current() const;  // nullptr if empty
    bool currentEnabled() const;

private:
    void step(int direction);  // move cursor to the next enabled item in a direction

    std::vector<MenuItem> items_;
    int cursor_ = 0;
    bool focusDisabled_ = false;
};

}  // namespace cd::ui
