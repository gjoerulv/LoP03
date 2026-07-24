#pragma once

#include "content/Enums.hpp"
#include "raylib.h"
#include "ui/UiStyle.hpp"

// M53: the accent colour for a weapon element chip, reusing existing M46 palette
// roles (no new palette entries). Pure and header-only so the equip shop and the
// unit tests share exactly one mapping. The battle target panel's weak/immune
// chips are the visual idiom this matches.

namespace cd::ui {

inline Color elementAccent(content::Element e, const style::Palette& p) {
    switch (e) {
        case content::Element::Fire: return p.danger;      // coral heat
        case content::Element::Ice: return p.crystal;      // cyan frost
        case content::Element::Lightning: return p.gold;   // bright spark
        case content::Element::Earth: return p.success;    // green stone/moss
        case content::Element::Holy: return p.gold;        // radiant gold
        case content::Element::Dark: return p.magic;       // violet
        case content::Element::None: break;
    }
    return p.textDim;  // unelemented: a muted chip, if ever shown
}

}  // namespace cd::ui
