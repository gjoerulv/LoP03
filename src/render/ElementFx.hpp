#pragma once

#include "content/Enums.hpp"

// M91: per-element impact accents (owner item 5) — small procedural motifs
// drawn over a hit unit during the sequencer's impact beat, in the M46
// stepped-pixel language (no textures, no RNG: everything derives from the
// passed strength, so captures and replays are stable). Presentation-only.
//
// `strength` is BattleSequencer::flashStrength() — already 0 when the Battle
// Flash setting is off, so the accessibility gate is inherited: 0 draws
// nothing. High contrast simplifies every motif to the palette's text color.

namespace cd::render {

void drawElementImpact(content::Element element, int cx, int cy, float strength,
                       bool highContrast);

}  // namespace cd::render
