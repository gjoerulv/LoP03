#pragma once

#include <array>

// M51 — the randomized comedic title phrase. Twelve original one-liners in the
// game's dry-comedy voice, one chosen per title-screen entry. Rules the phrases
// obey (pinned by test_ui_kit / a lint):
//  - the mandated line "Geese and Dragons; Spoons and Snacks!" is present;
//  - none describes the game's genre (that was the old tagline's job, now gone);
//  - each fits the title width at the body font.
// Pure data, so the lint runs headless.

namespace cd {

inline constexpr const char* kMandatedTitlePhrase = "Geese and Dragons; Spoons and Snacks!";

inline constexpr std::array<const char*, 12> kTitlePhrases{{
    "Geese and Dragons; Spoons and Snacks!",
    "Mind the spoon.",
    "The goose is not sorry.",
    "Taxes: now a weapon.",
    "The Jester was right about everything.",
    "Seven towns. One very hollow king.",
    "Do not feed the dungeon.",
    "Gerald would have LOVED this.",
    "As sung by storytellers, badly.",
    "Bring Snacks!",
    "The crown is real. Probably.",
    "Guards! Guards? Guards.",
}};

}  // namespace cd
