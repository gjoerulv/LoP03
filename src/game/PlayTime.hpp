#pragma once

#include <cstdint>
#include <string>

// M123 - the save slots' total play time, as pure data.
//
// A slot row shows the party's whole play time (the M109 ledger's
// `explore.playSeconds`) as seven digits, "HHH:MM:SS". The hour digits the
// party has not reached yet are drawn greyed - "001:03:56" greys its two
// leading zeros, a fresh party greys all three - so the clock keeps one fixed
// width in the list and still reads at a glance. Raylib-free by design (the
// QuitPrompt / EquipShopFilter precedent): the formatter is pinned headless
// and SlotMenuState only draws the two segments.

namespace cd {

inline constexpr std::int64_t kSlotPlayTimeCap = 999LL * 3600 + 59 * 60 + 59;

struct SlotPlayTime {
    std::string text;   // always "HHH:MM:SS" (nine characters)
    int greyChars = 0;  // leading hour digits not reached yet (0..3)
};

inline SlotPlayTime formatSlotPlayTime(std::int64_t seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    if (seconds > kSlotPlayTimeCap) {
        seconds = kSlotPlayTimeCap;  // the clock stops at 999:59:59, it never wraps
    }
    const int hours = static_cast<int>(seconds / 3600);
    const int minutes = static_cast<int>((seconds / 60) % 60);
    const int secs = static_cast<int>(seconds % 60);

    SlotPlayTime out;
    out.text.reserve(9);
    out.text += static_cast<char>('0' + hours / 100);
    out.text += static_cast<char>('0' + (hours / 10) % 10);
    out.text += static_cast<char>('0' + hours % 10);
    out.text += ':';
    out.text += static_cast<char>('0' + minutes / 10);
    out.text += static_cast<char>('0' + minutes % 10);
    out.text += ':';
    out.text += static_cast<char>('0' + secs / 10);
    out.text += static_cast<char>('0' + secs % 10);
    out.greyChars = hours >= 100 ? 0 : (hours >= 10 ? 1 : (hours >= 1 ? 2 : 3));
    return out;
}

}  // namespace cd
