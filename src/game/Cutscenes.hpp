#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "content/ContentDatabase.hpp"  // M100: the joke pool lives in content
#include "content/Definitions.hpp"
#include "game/BlackMarket.hpp"  // M110: blackMarketHash for the patrol-scene shuffle
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

// M100: the post-finale dry-joke pool — every authored "joke_*" scene id,
// sorted, so the cycle is deterministic regardless of map order. Empty when
// none are authored (the trigger site then simply replays the finale).
inline std::vector<std::string> strangerJokeIds(const content::ContentDatabase& db) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : db.cutscenes()) {
        (void)def;
        if (id.rfind(content::kJokeCutscenePrefix, 0) == 0) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// The joke the stranger tells on the Nth post-finale visit (told = how many
// have been told already): a plain cycle through the sorted pool — every joke
// heard before any repeats, reload-honest via the persisted counter.
inline std::string nextStrangerJokeId(const content::ContentDatabase& db, int told) {
    const std::vector<std::string> ids = strangerJokeIds(db);
    if (ids.empty()) {
        return {};
    }
    const std::size_t n = ids.size();
    const std::size_t index = static_cast<std::size_t>(told < 0 ? 0 : told) % n;
    return ids[index];
}

// M103: the dungeon stranger-story pool — every authored "story_*" scene id,
// sorted (same shape as the joke pool; timing-neutral tales for the in-run
// event, told whether or not the finale has happened).
inline std::vector<std::string> strangerStoryIds(const content::ContentDatabase& db) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : db.cutscenes()) {
        (void)def;
        if (id.rfind(content::kStoryCutscenePrefix, 0) == 0) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// M110: the Stranger's PATROL pool — every authored "patrol_*" scene id,
// sorted (the joke/story shape): optionless tales told when the danger
// counter's patrol resolves as a Stranger encounter, valid before or after
// the King. Empty when none are authored.
inline std::vector<std::string> strangerPatrolIds(const content::ContentDatabase& db) {
    std::vector<std::string> ids;
    for (const auto& [id, def] : db.cutscenes()) {
        (void)def;
        if (id.rfind(content::kPatrolCutscenePrefix, 0) == 0) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// The scene for a run's Nth patrol encounter (told = how many the run has
// met already): the sorted pool in a seed-shuffled order — a Fisher-Yates
// over pure hashes of the run seed under its own salt — walked to the end
// before it repeats, so a long run meets every scene once per cycle. Reload-
// honest by construction; never touches the joke or story counters. "" when
// the pool is empty (the trigger then falls back to an ordinary patrol).
inline std::string patrolSceneFor(const content::ContentDatabase& db, std::uint64_t runSeed,
                                  int told) {
    std::vector<std::string> ids = strangerPatrolIds(db);
    if (ids.empty()) {
        return {};
    }
    constexpr std::uint64_t kSaltPatrolScene = 0x7A7201C5CE0E5EEDull;
    for (std::size_t i = ids.size() - 1; i > 0; --i) {
        const std::uint64_t h =
            blackMarketHash(runSeed, kSaltPatrolScene + static_cast<std::uint64_t>(i));
        const std::size_t j = static_cast<std::size_t>(h % (i + 1));
        std::swap(ids[i], ids[j]);
    }
    return ids[static_cast<std::size_t>(told < 0 ? 0 : told) % ids.size()];
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
