#pragma once

#include <array>
#include <cstddef>

#include "render/RaylibRAII.hpp"

// Placeholder audio: simple SFX and looping music are SYNTHESIZED at runtime, so
// no asset files are required. Everything is guarded by the audio device state —
// if init or generation fails, all calls are silent no-ops (never a crash).
// Dropping real files in `assets/audio/` is the intended later replacement path.

namespace cd {

enum class Sfx { Move, Confirm, Cancel, Hit, Heal, Ko, Victory, Defeat, Chest };
inline constexpr std::size_t kSfxCount = 9;

enum class MusicTrack { None, Town, Dungeon, Battle };
inline constexpr std::size_t kMusicCount = 3;  // Town, Dungeon, Battle

class AudioManager {
public:
    AudioManager();

    void play(Sfx id);
    void setMusic(MusicTrack track);
    void update();  // call each frame; loops the current track
    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    // Volume controls (0..1 each), persisted via settings (M13). Master is a
    // multiplier on the manager's base output level; music/sfx scale their
    // sound groups.
    void setVolumes(float master, float music, float sfx);

private:
    // RAII for the audio device; first member so it outlives the sound handles.
    struct DeviceGuard {
        DeviceGuard();
        ~DeviceGuard();
        DeviceGuard(const DeviceGuard&) = delete;
        DeviceGuard& operator=(const DeviceGuard&) = delete;
    };

    DeviceGuard device_;
    bool ready_ = false;
    bool enabled_ = true;
    std::array<SoundHandle, kSfxCount> sfx_;
    std::array<SoundHandle, kMusicCount> music_;
    MusicTrack current_ = MusicTrack::None;
};

}  // namespace cd
