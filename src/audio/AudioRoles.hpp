#pragma once

#include <array>
#include <cstddef>

// Logical audio roles and pure audio policy (M21). This header is
// raylib-free so headless tests can validate the role tables, the shipped
// manifest coverage, and the rate-limit policy without an audio device.
//
// Swapping the sound behind any role requires only an assets/manifest.json
// entry + file — the ids below are the stable contract.

namespace cd {

enum class Sfx {
    Move,
    Confirm,
    Cancel,
    Error,
    Hit,
    HitMagic,
    Heal,
    Status,
    Ko,
    Victory,
    Defeat,
    Chest,
    Step,
    Door,
    Interact,
};
inline constexpr std::size_t kSfxCount = 15;

enum class MusicTrack {
    None,
    Title,
    Town,
    Guild,
    DungeonKeep,
    DungeonMine,
    DungeonForest,
    Battle,
    Boss,
    Victory,
    Defeat,
    Result,
    Castle,       // M40: the castle place
    KingBattle,   // M40: the King fight
};
inline constexpr std::size_t kMusicCount = 13;  // excludes None

enum class AmbienceTrack {
    None,
    Town,
    Keep,
    Mine,
    Forest,
};
inline constexpr std::size_t kAmbienceCount = 4;  // excludes None

namespace audio {

inline constexpr std::array<const char*, kSfxCount> kSfxIds = {
    "sfx.ui.move",       "sfx.ui.confirm",       "sfx.ui.cancel",   "sfx.ui.error",
    "sfx.battle.hit",    "sfx.battle.hit_magic", "sfx.battle.heal", "sfx.battle.status",
    "sfx.battle.ko",     "sfx.battle.victory",   "sfx.battle.defeat",
    "sfx.world.chest",   "sfx.world.step",       "sfx.world.door",  "sfx.world.interact",
};

inline constexpr std::array<const char*, kMusicCount> kMusicIds = {
    "music.title",         "music.town",         "music.guild",
    "music.dungeon.keep",  "music.dungeon.mine", "music.dungeon.forest",
    "music.battle",        "music.boss",         "music.victory",
    "music.defeat",        "music.result",       "music.castle",
    "music.king",
};

inline constexpr std::array<const char*, kAmbienceCount> kAmbienceIds = {
    "ambience.town",
    "ambience.keep",
    "ambience.mine",
    "ambience.forest",
};

// Victory/defeat are one-shot jingles: the music channel plays them once and
// then falls silent until the next scene sets a track. When the jingle file
// is missing, the matching battle stinger SFX plays instead so battle end is
// never silent.
inline constexpr bool isJingle(MusicTrack t) {
    return t == MusicTrack::Victory || t == MusicTrack::Defeat;
}

// Which of the three M8 synthesized loops (0 town, 1 dungeon, 2 battle)
// backs each role when its file is missing; -1 = no synthesized tier.
inline constexpr std::array<int, kMusicCount> kSynthMusicIndex = {
    0,          // Title
    0,          // Town
    0,          // Guild
    1,  1,  1,  // DungeonKeep / DungeonMine / DungeonForest
    2,          // Battle
    2,          // Boss
    -1, -1,     // Victory / Defeat (jingles fall back to stinger SFX)
    0,          // Result
    0,          // Castle (town-tier synth fallback)
    2,          // KingBattle (battle-tier synth fallback)
};

// Minimum seconds between accepted plays of the same SFX role. Guards rapid
// menu scrolling and per-tile step spam without ever swallowing a first play.
inline constexpr std::array<float, kSfxCount> kSfxMinInterval = {
    0.05f, 0.06f, 0.06f, 0.10f,  // move, confirm, cancel, error
    0.05f, 0.05f, 0.05f, 0.05f,  // hit, hit_magic, heal, status
    0.05f, 0.25f, 0.25f,         // ko, victory, defeat
    0.10f, 0.16f, 0.12f, 0.10f,  // chest, step, door, interact
};

// Pure rate-limit decision; `last` is the time of the previous accepted play
// (negative = never played). Callers record `now` on accept.
inline bool sfxAllowed(Sfx id, double now, double last) {
    if (last < 0.0) {
        return true;
    }
    return now - last >= static_cast<double>(kSfxMinInterval[static_cast<std::size_t>(id)]);
}

// Outgoing music streams fade over this window while the next track starts.
inline constexpr float kMusicFadeSeconds = 0.25f;

inline constexpr float fadeGain(float elapsed) {
    if (elapsed <= 0.0f) {
        return 1.0f;
    }
    if (elapsed >= kMusicFadeSeconds) {
        return 0.0f;
    }
    return 1.0f - elapsed / kMusicFadeSeconds;
}

}  // namespace audio
}  // namespace cd
