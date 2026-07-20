#pragma once

#include <array>
#include <cstddef>
#include <filesystem>

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
