#pragma once

#include <array>

// M71 — the celebration punchline: one dry, lore-friendly line under the
// score, picked at random per celebration (the TitlePhrases idiom). Rules the
// pool obeys (pinned by a headless lint in test_presentation_options):
//  - every line is original, in the game's dry voice, and references only the
//    game's own world (spoons, geese, the Guild, the bard, the Hollow King);
//  - none describes the genre;
//  - each fits the celebration width at the drawn font.
// Pure data, so the lint runs headless.

namespace cd {

inline constexpr std::array<const char*, 12> kCelebrationPhrases{{
    "The dungeon files no complaint.",
    "The Guild counted every turn. Twice.",
    "The bard will exaggerate this.",
    "The spoon claims full credit.",
    "Somewhere, a goose approves.",
    "The crystals saw everything.",
    "Paperwork to follow.",
    "Add it to the ballad.",
    "Snacks are earned.",
    "The King would call it luck.",
    "No gate held. As usual.",
    "Recorded. Filed. Celebrated.",
}};

}  // namespace cd
