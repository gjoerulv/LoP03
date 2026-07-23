#pragma once

#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "settings/Settings.hpp"

// M51 — the faint full-screen tint drawn during the impact beat of an all-target
// action. Pure classification + alpha math, so both are unit-tested headlessly;
// BattleState only decides the color and draws the rect.

namespace cd {

// What flavour of all-target action is being shown (drives the tint colour).
// None means "not an all-target action" -> no tint.
enum class AoeTint { None, Heal, Damage, Debuff };

// The tint for a resolved SKILL: only all_allies / all_enemies skills tint.
//  - all_allies  -> Heal   (a party-wide heal or buff; success colour)
//  - all_enemies, power > 0 -> Damage (a mass attack; danger colour)
//  - all_enemies, power 0 with a status -> Debuff (a mass curse; magic colour)
inline AoeTint aoeTintForSkill(const content::SkillDef& s) {
    if (s.target == content::SkillTarget::AllAllies) {
        return AoeTint::Heal;
    }
    if (s.target == content::SkillTarget::AllEnemies) {
        if (s.power > 0) {
            return AoeTint::Damage;
        }
        if (s.statusEffect != content::StatusType::None) {
            return AoeTint::Debuff;
        }
        return AoeTint::Damage;  // a 0-power all-enemies skill still reads as an assault
    }
    return AoeTint::None;
}

// The alpha of the tint rect: 0.12 at full flash strength, scaled by the flash
// setting (Off -> 0, Reduced -> half, Full -> as given). `flashStrength` is the
// sequencer's 0..1 impact pulse, so the tint is a single decay, never a strobe.
inline float aoeTintAlpha(AoeTint tint, float flashStrength, settings::EffectLevel flash) {
    if (tint == AoeTint::None || flash == settings::EffectLevel::Off || flashStrength <= 0.0f) {
        return 0.0f;
    }
    const float cap = 0.12f;
    const float scale = flash == settings::EffectLevel::Reduced ? 0.5f : 1.0f;
    float a = cap * flashStrength * scale;
    return a < 0.0f ? 0.0f : (a > cap ? cap : a);
}

}  // namespace cd
