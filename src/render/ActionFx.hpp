#pragma once

#include "content/Enums.hpp"
#include "raylib.h"
#include "render/ActionTier.hpp"

// M130: the short attack and skill animations — procedural stepped-pixel
// motifs in the M91/M107 idiom (no textures, no RNG: everything derives
// from the family, the tier and the clock, so captures and replays are
// stable). A cast glyph rises over the actor during the windup; a burst
// plays on every affected unit from the impact beat, 3–6 frames at the
// 80s cadence, then holds and fades through its tail inside the settle
// pause. High contrast collapses every motif to the palette's text colour
// so shape alone carries the read; the Battle Flash gate is applied by the
// caller (nothing is drawn when the setting is Off). Presentation only.

namespace cd::render {

// The colour a family reads in (the M91 element hues; neutral = text,
// heal = success green, buff = gold, debuff = magic violet).
Color actionFamilyColor(content::SkillKind kind, bool highContrast);

// The windup glyph over the caster's head: `t` seconds since the sequence
// began. Spells and heals only; a strike shows nothing but its lunge.
void drawActionCast(content::SkillKind kind, ActionTier tier, int cx, int topY, float t,
                    bool highContrast);

// The burst over one affected unit: `t` seconds since the impact beat (a
// negative t draws nothing; past the burst's lifetime nothing is drawn).
void drawActionBurst(content::SkillKind kind, ActionTier tier, int cx, int cy, float t,
                     bool highContrast);

}  // namespace cd::render
