#pragma once

#include <algorithm>
#include <cstddef>
#include <string>

#include "game/Party.hpp"

// M97: pure rules for the Hooded Goose story scenes — seen-tracking, the
// scene<->town mapping, the recorded heirloom choices, and the name-token
// substitution. No raylib, no JSON; the CutsceneState and the trigger sites
// (PartyCreationState, TownState) all read these so live play and tests agree.

namespace cd::game {

// The scene played on first arrival at `town` (2..7), or "" for any other town
// (town 1 is the new_game prologue's stage; the finale is NPC-triggered).
inline std::string townCutsceneId(int town) {
    if (town < 2 || town > 7) {
        return {};
    }
    return "town_" + std::to_string(town);
}

inline bool cutsceneSeen(const Party& party, const std::string& sceneId) {
    return std::find(party.seenCutscenes.begin(), party.seenCutscenes.end(), sceneId) !=
           party.seenCutscenes.end();
}

// Marks before the scene pushes (the M41 storyMet idiom), so a reload of a
// save written afterwards can never replay it. Idempotent.
inline void markCutsceneSeen(Party& party, const std::string& sceneId) {
    if (!cutsceneSeen(party, sceneId)) {
        party.seenCutscenes.push_back(sceneId);
    }
}

// Choices persist as "scene:heirloom" strings (one per scene, additive save
// field). The colon can appear in neither id (loader ids are snake_case), so
// the encoding is unambiguous.
inline std::string encodeCutsceneChoice(const std::string& sceneId, const std::string& heirloomId) {
    return sceneId + ":" + heirloomId;
}

// Splits an encoded choice; returns false (and touches nothing) when the
// entry is malformed — the save reader drops those instead of guessing.
inline bool decodeCutsceneChoice(const std::string& entry, std::string& sceneId,
                                 std::string& heirloomId) {
    const std::size_t colon = entry.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= entry.size()) {
        return false;
    }
    sceneId = entry.substr(0, colon);
    heirloomId = entry.substr(colon + 1);
    return true;
}

// The heirloom this party already chose in `sceneId`, or "" when the choice
// has not been made — the grant fires only while this is empty, which is what
// makes finale replays (and debug plays) safe.
inline std::string cutsceneChoiceFor(const Party& party, const std::string& sceneId) {
    for (const std::string& entry : party.heirloomChoices) {
        std::string scene;
        std::string heirloom;
        if (decodeCutsceneChoice(entry, scene, heirloom) && scene == sceneId) {
            return heirloom;
        }
    }
    return {};
}

inline void recordCutsceneChoice(Party& party, const std::string& sceneId,
                                 const std::string& heirloomId) {
    if (cutsceneChoiceFor(party, sceneId).empty()) {
        party.heirloomChoices.push_back(encodeCutsceneChoice(sceneId, heirloomId));
    }
}

// Replaces the {member1}..{member4} name tokens with the party's member names.
// A token past the roster's end stays literal (defensive; the shipped party is
// always four) — never a crash, never an empty splice.
inline std::string cutsceneResolveTokens(std::string text, const Party& party) {
    for (std::size_t i = 0; i < kMaxPartySize; ++i) {
        const std::string token = "{member" + std::to_string(i + 1) + "}";
        if (i >= party.members.size()) {
            continue;
        }
        std::size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos) {
            text.replace(pos, token.size(), party.members[i].name);
            pos += party.members[i].name.size();
        }
    }
    return text;
}

}  // namespace cd::game
