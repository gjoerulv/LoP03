#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

#include "audio/AudioRoles.hpp"
#include "render/RaylibRAII.hpp"

// Audio with two tiers per logical role (M14, owner-approved fallback order):
// a file from the asset manifest when one is present and loads, otherwise the
// synthesized placeholder tone, otherwise silence. Everything stays guarded by
// the audio device state — if init or any load fails, calls are silent no-ops
// (never a crash). File-backed music uses raylib music streams; synthesized
// music keeps the M8 re-play loop. Replacing any sound requires only an
// assets/manifest.json entry + file — no C++ (role ids in AudioRoles.hpp).
//
// M21 additions: a second streamed channel for ambience (file-or-silence;
// governed by the SFX volume since M27), a short crossfade when the music track
// changes, one-shot victory/defeat jingles (stinger SFX fallback), and
// per-role SFX rate limiting for rapid menus and footsteps.

namespace cd {

namespace assets {
class AssetManifest;
}

class AudioManager {
public:
    AudioManager();

    void play(Sfx id);
    void setMusic(MusicTrack track);
    // Selects which town-music variant backs MusicTrack::Town (M32). Town 1 is
    // the base `music.town`; towns 2..7 use `music.town.<n>` when present, else
    // fall back to the base track. Rebinds the Town stream and, if Town music is
    // playing, restarts it on the new variant. No-op when the town is unchanged.
    void setTown(int town);
    void setAmbience(AmbienceTrack track);
    void update();  // call each frame; streams, loops, and fades
    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    // The active scene tracks (also valid when the audio device is absent —
    // set*/currentAmbience are tracked regardless). Used by tests/diagnostics.
    MusicTrack currentMusic() const { return current_; }
    AmbienceTrack currentAmbience() const { return currentAmbience_; }

    // Volume controls (0..1 each), persisted via settings (M13). Applied to
    // the master output, the current music and ambience streams, and each SFX
    // at play time. Ambience follows the SFX slider (M27).
    void setVolumes(float master, float music, float sfx);

    // Resolves every audio role against the manifest (M14): file-backed
    // sounds/streams replace the synthesized tier where present and loadable.
    // Safe to call again for the debug reload — the current tracks restart.
    void applyManifest(const assets::AssetManifest& manifest,
                       const std::filesystem::path& root);

private:
    // RAII for the audio device; first member so it outlives the sound handles.
    struct DeviceGuard {
        DeviceGuard();
        ~DeviceGuard();
        DeviceGuard(const DeviceGuard&) = delete;
        DeviceGuard& operator=(const DeviceGuard&) = delete;
    };

    static constexpr std::size_t kSynthMusicCount = 3;  // town, dungeon, battle

    void stopCurrent();
    void startCurrent();
    void stopAmbience();
    void startAmbience();
    void cancelFade();
    // (Re)binds the MusicTrack::Town file stream to the current town variant
    // (M32); safe no-op fallback to synth when neither file resolves.
    void loadTownMusicSlot();

    static constexpr std::size_t kTownMusicVariants = 7;  // matches kTownCount

    DeviceGuard device_;
    bool ready_ = false;
    bool enabled_ = true;

    // Synthesized tier (always built when the device is ready).
    std::array<SoundHandle, kSfxCount> synthSfx_;
    std::array<SoundHandle, kSynthMusicCount> synthMusic_;

    // File tier from the manifest (optional per role).
    std::array<SoundHandle, kSfxCount> fileSfx_;
    std::array<float, kSfxCount> fileSfxVolume_{};
    std::array<MusicHandle, kMusicCount> fileMusic_;
    std::array<float, kMusicCount> fileMusicVolume_{};
    std::array<MusicHandle, kAmbienceCount> fileAmbience_;
    std::array<float, kAmbienceCount> fileAmbienceVolume_{};

    MusicTrack current_ = MusicTrack::None;
    AmbienceTrack currentAmbience_ = AmbienceTrack::None;

    // Town-music variants (M32): the active town, the manifest-resolved relative
    // path + volume per town (index 0 = town 1 = base `music.town`), and the
    // assets root so a variant can be (re)loaded on a town switch.
    int townMusic_ = 1;
    std::filesystem::path assetsRoot_;
    std::array<std::string, kTownMusicVariants> townMusicRel_{};
    std::array<float, kTownMusicVariants> townMusicVol_{};

    // Crossfade state: index into fileMusic_ still fading out, or -1.
    int fadingIndex_ = -1;
    float fadeElapsed_ = 0.0f;

    // Last accepted play time per SFX role (negative = never), for the pure
    // rate-limit policy in AudioRoles.hpp.
    std::array<double, kSfxCount> lastSfxTime_{};

    float masterVolume_ = 1.0f;
    float musicVolume_ = 1.0f;
    float sfxVolume_ = 1.0f;
};

}  // namespace cd
