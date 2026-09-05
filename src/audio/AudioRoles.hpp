#pragma once

#include <array>
#include <cstddef>

#include "content/Enums.hpp"  // M91: Element -> impact-role mapping

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
    // M91: per-element battle impacts, appended so every earlier role keeps
    // its table index. One per content::Element (None keeps HitMagic).
    HitFire,
    HitIce,
    HitLightning,
    HitEarth,
    HitHoly,
    HitDark,
    // M107: the summons' arrival fanfare (one shared voice for the three
    // legends; per-summon voices are the owner's call for a later pass).
    Summon,
};
inline constexpr std::size_t kSfxCount = 22;

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
    DuckBattle,   // M62: the Deadly Duck fight (his pond, his own anthem)
    // Appended (owner direction 2026-08-17) so every earlier role keeps its
    // table index: the Goosy Gauntlet's own tune.
    DungeonGoosy,
    // M112: the Jester's mocking jingle — a one-shot like Victory/Defeat
    // (appended, so every earlier role keeps its table index).
    Mock,
};
inline constexpr std::size_t kMusicCount = 16;  // excludes None

enum class AmbienceTrack {
    None,
    Town,
    Keep,
    Mine,
    Forest,
    Goosy,  // owner direction 2026-08-17: the wetland bed
};
inline constexpr std::size_t kAmbienceCount = 5;  // excludes None

namespace audio {

inline constexpr std::array<const char*, kSfxCount> kSfxIds = {
    "sfx.ui.move",       "sfx.ui.confirm",       "sfx.ui.cancel",   "sfx.ui.error",
    "sfx.battle.hit",    "sfx.battle.hit_magic", "sfx.battle.heal", "sfx.battle.status",
    "sfx.battle.ko",     "sfx.battle.victory",   "sfx.battle.defeat",
    "sfx.world.chest",   "sfx.world.step",       "sfx.world.door",  "sfx.world.interact",
    // M91: the six elemental impacts.
    "sfx.battle.hit_fire",  "sfx.battle.hit_ice",  "sfx.battle.hit_lightning",
    "sfx.battle.hit_earth", "sfx.battle.hit_holy", "sfx.battle.hit_dark",
    // M107: the summon arrival.
    "sfx.battle.summon",
};

inline constexpr std::array<const char*, kMusicCount> kMusicIds = {
    "music.title",         "music.town",         "music.guild",
    "music.dungeon.keep",  "music.dungeon.mine", "music.dungeon.forest",
    "music.battle",        "music.boss",         "music.victory",
    "music.defeat",        "music.result",       "music.castle",
    "music.king",          "music.duck",         "music.dungeon.goosy",
    "music.mock",
};

inline constexpr std::array<const char*, kAmbienceCount> kAmbienceIds = {
    "ambience.town",
    "ambience.keep",
    "ambience.mine",
    "ambience.forest",
    "ambience.goosy",
};

// Victory/defeat are one-shot jingles: the music channel plays them once and
// then falls silent until the next scene sets a track. When the jingle file
// is missing, the matching battle stinger SFX plays instead so battle end is
// never silent.
inline constexpr bool isJingle(MusicTrack t) {
    return t == MusicTrack::Victory || t == MusicTrack::Defeat || t == MusicTrack::Mock;  // M112
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
    2,          // DuckBattle (battle-tier synth fallback)
    1,          // DungeonGoosy (dungeon-tier synth fallback)
    -1,         // Mock (M112: a jingle; falls back to the error stinger)
};

// Minimum seconds between accepted plays of the same SFX role. Guards rapid
// menu scrolling and per-tile step spam without ever swallowing a first play.
inline constexpr std::array<float, kSfxCount> kSfxMinInterval = {
    0.05f, 0.06f, 0.06f, 0.10f,  // move, confirm, cancel, error
    0.05f, 0.05f, 0.05f, 0.05f,  // hit, hit_magic, heal, status
    0.05f, 0.25f, 0.25f,         // ko, victory, defeat
    0.10f, 0.16f, 0.12f, 0.10f,  // chest, step, door, interact
    0.05f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f,  // M91: the elemental impacts
    0.30f,                                     // M107: the summon arrival (one-shot beat)
};

// M91: the impact role for an elemental hit. None (and any future value)
// keeps the generic magic hit, so an unmapped element can never go silent.
inline Sfx elementHitSfx(content::Element e) {
    switch (e) {
        case content::Element::Fire: return Sfx::HitFire;
        case content::Element::Ice: return Sfx::HitIce;
        case content::Element::Lightning: return Sfx::HitLightning;
        case content::Element::Earth: return Sfx::HitEarth;
        case content::Element::Holy: return Sfx::HitHoly;
        case content::Element::Dark: return Sfx::HitDark;
        case content::Element::None: break;
    }
    return Sfx::HitMagic;
}

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
