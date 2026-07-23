#pragma once

#include <string>

#include "content/Stats.hpp"

// M52 — equip stat-bonus formatting, as pure functions.
//
// Promoted out of EquipShopState (M31/M22 helpers) so the equip-shop Buy detail,
// the Details overlay, and the new equip-flow diff all share one rule, and so
// the formatting is unit-tested headlessly. Raylib-free.

namespace cd::equip {

// "HP+20 ATK+6" style absolute summary of one equipment bonus; empty when the
// bonus is all zero.
inline std::string statBonusSummary(const content::StatBlock& b) {
    std::string out;
    const auto add = [&out](const char* tag, int v) {
        if (v != 0) {
            out += (out.empty() ? "" : " ") + std::string(tag) + (v > 0 ? "+" : "") +
                   std::to_string(v);
        }
    };
    add("HP", b.maxHp);
    add("ATK", b.attack);
    add("MAG", b.magic);
    add("DEF", b.defense);
    add("SPD", b.speed);
    return out;
}

// "ATK +2  DEF -1" style per-field delta between two equipment bonuses (next vs
// current); empty when there is no change.
inline std::string bonusDelta(const content::StatBlock& next, const content::StatBlock& cur) {
    std::string out;
    const auto add = [&out](const char* tag, int d) {
        if (d == 0) {
            return;
        }
        if (!out.empty()) {
            out += "  ";
        }
        out += std::string(tag) + (d > 0 ? " +" : " ") + std::to_string(d);
    };
    add("HP", next.maxHp - cur.maxHp);
    add("ATK", next.attack - cur.attack);
    add("MAG", next.magic - cur.magic);
    add("DEF", next.defense - cur.defense);
    add("SPD", next.speed - cur.speed);
    return out;
}

// Net sign of a diff, so its colour is chosen in one place: +1 when the summed
// stat change is positive, -1 when negative, 0 when it nets out (or is empty).
// The +/- signs in bonusDelta carry the per-stat truth; this only tints.
inline int deltaSign(const content::StatBlock& next, const content::StatBlock& cur) {
    const int sum = (next.maxHp - cur.maxHp) + (next.attack - cur.attack) +
                    (next.magic - cur.magic) + (next.defense - cur.defense) +
                    (next.speed - cur.speed);
    return sum > 0 ? 1 : (sum < 0 ? -1 : 0);
}

}  // namespace cd::equip
