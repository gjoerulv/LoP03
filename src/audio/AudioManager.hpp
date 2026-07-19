#pragma once

#include <array>
#include <cstddef>
#include <filesystem>

#include "render/RaylibRAII.hpp"

// Audio with two tiers per logical role (M14, owner-approved fallback order):
// a file from the asset manifest when one is present and loads, otherwise the
// synthesized placeholder tone, otherwise silence. Everything stays guarded by
// the audio device state — if init or any load fails, calls are silent no-ops
// (never a crash). File-backed music uses raylib music streams; synthesized
// music keeps the M8 re-play loop. Replacing any sound requires only an
// assets/manifest.json entry + file — no C++ (roles below).
//
// Manifest role ids: sfx.ui.{move,confirm,cancel}, sfx.battle.{hit,heal,ko,
// victory,defeat}, sfx.world.chest, music.{town,dungeon,battle}.

namespace cd {

namespace assets {
class AssetManifest;
}

enum class Sfx { Move, Confirm, Cancel, Hit, Heal, Ko, Victory, Defeat, Chest };
inline constexpr std::size_t kSfxCount = 9;

enum class MusicTrack { None, Town, Dungeon, Battle };
inline constexpr std::size_t kMusicCount = 3;  // Town, Dungeon, Battle

class AudioManager {
public:
    AudioManager();

    void play(Sfx id);
    void setMusic(MusicTrack track);
    void update();  // call each frame; streams/loops the current track
    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    // Volume controls (0..1 each), persisted via settings (M13). Applied to
    // the master output, the current music, and each SFX at play time.
    void setVolumes(float master, float music, float sfx);

    // Resolves every audio role against the manifest (M14): file-backed
    // sounds/streams replace the synthesized tier where present and loadable.
    // Safe to call again for the debug reload — the current track restarts.
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

    void stopCurrent();
    void startCurrent();

    DeviceGuard device_;
    bool ready_ = false;
    bool enabled_ = true;

    // Synthesized tier (always built when the device is ready).
    std::array<SoundHandle, kSfxCount> synthSfx_;
    std::array<SoundHandle, kMusicCount> synthMusic_;

    // File tier from the manifest (optional per role).
    std::array<SoundHandle, kSfxCount> fileSfx_;
    std::array<float, kSfxCount> fileSfxVolume_{};
    std::array<MusicHandle, kMusicCount> fileMusic_;
    std::array<float, kMusicCount> fileMusicVolume_{};

    MusicTrack current_ = MusicTrack::None;
    float masterVolume_ = 1.0f;
    float musicVolume_ = 1.0f;
    float sfxVolume_ = 1.0f;
};

}  // namespace cd
