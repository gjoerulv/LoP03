#pragma once

#include <array>

// M124 - the one dry line under "Defeat!" on the Iron Man's send-off (the
// TitlePhrases / CelebrationPhrases idiom: original text, presentation only,
// drawn with GetRandomValue so the capture stays reproducible). Every line
// must fit the 426 px screen at body size - test_presentation_options pins
// the width headlessly, as it does for the celebration's.

namespace cd {

inline constexpr std::array<const char*, 16> kFallenPhrases = {
    "The geese would like it known that they barely tried.",
    "Pecked. Honked at. Pecked again. In that order.",
    "The King wants this part replayed at dinner.",
    "Somewhere, a duck is telling this story better.",
    "No saves were harmed. There were none.",
    "Iron, it turns out, dents.",
    "The ducks took turns. It was very civil.",
    "A memorial will be held. The geese are catering.",
    "History will remember. The King will do impressions.",
    "They came, they saw, they were sat upon by waterfowl.",
    "The bards are already on the unflattering verses.",
    "One sitting, as promised. Mostly spent lying down.",
    "The flock has left a review. One star. Would peck again.",
    "It is rude to laugh. The King is being very rude.",
    "Permadeath: now with extra feathers.",
    "They fought like heroes. The geese fought like geese.",
};

}  // namespace cd
