#include "audio/AudioManager.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "assets/AssetManifest.hpp"
#include "core/Log.hpp"
#include "raylib.h"

namespace cd {

namespace {

constexpr int kSampleRate = 22050;
constexpr float kPi = 3.14159265358979323846f;
// The synthesized tones are deliberately played quieter than real files (they
// are harsh); this preserves the pre-M14 loudness now that the master volume
// uses the full range.
constexpr float kSynthLevel = 0.5f;

// Manifest role ids, indexed like the Sfx / MusicTrack-1 enums.
constexpr const char* kSfxIds[kSfxCount] = {
    "sfx.ui.move",       "sfx.ui.confirm",    "sfx.ui.cancel",
    "sfx.battle.hit",    "sfx.battle.heal",   "sfx.battle.ko",
    "sfx.battle.victory", "sfx.battle.defeat", "sfx.world.chest",
};
constexpr const char* kMusicIds[kMusicCount] = {"music.town", "music.dungeon", "music.battle"};

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// Tiny deterministic noise source (for hit SFX) — no <random> needed.
float noiseSample(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (static_cast<float>(state & 0xFFFF) / 32768.0f) - 1.0f;
}

void appendTone(std::vector<short>& out, float freq, float seconds, float volume, bool noise,
                float freqEnd) {
    const int n = static_cast<int>(static_cast<float>(kSampleRate) * seconds);
    std::uint32_t rng = 0x1234567u;
    for (int i = 0; i < n; ++i) {
        const float frac = n > 0 ? static_cast<float>(i) / static_cast<float>(n) : 0.0f;
        const float env = 1.0f - frac;  // linear decay
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        const float f = freqEnd > 0.0f ? freq + (freqEnd - freq) * frac : freq;
        const float value = noise ? noiseSample(rng) : std::sin(2.0f * kPi * f * t);
        out.push_back(static_cast<short>(volume * env * value * 30000.0f));
    }
}

SoundHandle soundFromSamples(const std::vector<short>& samples) {
    if (samples.empty() || !IsAudioDeviceReady()) {
        return {};
    }
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(samples.size());
    wave.sampleRate = static_cast<unsigned int>(kSampleRate);
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = const_cast<short*>(samples.data());
    const Sound sound = LoadSoundFromWave(wave);
    if (!IsSoundValid(sound)) {
        return {};
    }
    return SoundHandle(sound);
}

SoundHandle makeTone(float freq, float seconds, float volume, bool noise = false,
                     float freqEnd = -1.0f) {
    std::vector<short> samples;
    appendTone(samples, freq, seconds, volume, noise, freqEnd);
    return soundFromSamples(samples);
}

SoundHandle makeLoop(const std::vector<float>& notes, float noteDur, int repeats, float volume) {
    std::vector<short> samples;
    for (int r = 0; r < repeats; ++r) {
        for (float note : notes) {
            appendTone(samples, note, noteDur, volume, false, -1.0f);
        }
    }
    return soundFromSamples(samples);
}

}  // namespace

AudioManager::DeviceGuard::DeviceGuard() { InitAudioDevice(); }
AudioManager::DeviceGuard::~DeviceGuard() {
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
}

AudioManager::AudioManager() {
    ready_ = IsAudioDeviceReady();
    if (!ready_) {
        return;
    }
    SetMasterVolume(masterVolume_);

    synthSfx_[static_cast<std::size_t>(Sfx::Move)] = makeTone(880.0f, 0.04f, 0.3f);
    synthSfx_[static_cast<std::size_t>(Sfx::Confirm)] =
        makeTone(660.0f, 0.09f, 0.4f, false, 990.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Cancel)] =
        makeTone(440.0f, 0.09f, 0.35f, false, 300.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Hit)] = makeTone(0.0f, 0.10f, 0.4f, true);
    synthSfx_[static_cast<std::size_t>(Sfx::Heal)] =
        makeTone(523.0f, 0.18f, 0.35f, false, 784.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Ko)] = makeTone(330.0f, 0.25f, 0.4f, false, 120.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Victory)] =
        makeTone(523.0f, 0.35f, 0.45f, false, 1046.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Defeat)] =
        makeTone(220.0f, 0.45f, 0.4f, false, 90.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Chest)] =
        makeTone(784.0f, 0.12f, 0.4f, false, 1175.0f);

    // ~2s looping placeholder tracks.
    synthMusic_[0] = makeLoop({523.0f, 659.0f, 784.0f, 659.0f}, 0.22f, 2, 0.12f);  // Town
    synthMusic_[1] = makeLoop({392.0f, 466.0f, 523.0f, 466.0f}, 0.28f, 2, 0.12f);  // Dungeon
    synthMusic_[2] = makeLoop({440.0f, 554.0f, 659.0f, 554.0f}, 0.16f, 3, 0.13f);  // Battle
}

void AudioManager::play(Sfx id) {
    if (!ready_ || !enabled_) {
        return;
    }
    const std::size_t i = static_cast<std::size_t>(id);
    if (fileSfx_[i].valid()) {
        SetSoundVolume(fileSfx_[i].get(), sfxVolume_ * fileSfxVolume_[i]);
        PlaySound(fileSfx_[i].get());
        return;
    }
    if (synthSfx_[i].valid()) {
        SetSoundVolume(synthSfx_[i].get(), sfxVolume_ * kSynthLevel);
        PlaySound(synthSfx_[i].get());
    }
    // Neither tier available: silence (still no crash).
}

void AudioManager::stopCurrent() {
    if (!ready_ || current_ == MusicTrack::None) {
        return;
    }
    const std::size_t idx = static_cast<std::size_t>(current_) - 1;
    if (idx >= kMusicCount) {
        return;
    }
    if (fileMusic_[idx].valid()) {
        StopMusicStream(fileMusic_[idx].get());
    } else if (synthMusic_[idx].valid()) {
        StopSound(synthMusic_[idx].get());
    }
}

void AudioManager::startCurrent() {
    if (!ready_ || !enabled_ || current_ == MusicTrack::None) {
        return;
    }
    const std::size_t idx = static_cast<std::size_t>(current_) - 1;
    if (idx >= kMusicCount) {
        return;
    }
    if (fileMusic_[idx].valid()) {
        SetMusicVolume(fileMusic_[idx].get(), musicVolume_ * fileMusicVolume_[idx]);
        PlayMusicStream(fileMusic_[idx].get());
    } else if (synthMusic_[idx].valid()) {
        SetSoundVolume(synthMusic_[idx].get(), musicVolume_ * kSynthLevel);
        PlaySound(synthMusic_[idx].get());
    }
}

void AudioManager::setMusic(MusicTrack track) {
    if (track == current_) {
        return;
    }
    stopCurrent();
    current_ = track;
    startCurrent();
}

void AudioManager::update() {
    if (!ready_ || !enabled_ || current_ == MusicTrack::None) {
        return;
    }
    const std::size_t idx = static_cast<std::size_t>(current_) - 1;
    if (idx >= kMusicCount) {
        return;
    }
    if (fileMusic_[idx].valid()) {
        UpdateMusicStream(fileMusic_[idx].get());  // looping handled by the stream
    } else if (synthMusic_[idx].valid() && !IsSoundPlaying(synthMusic_[idx].get())) {
        PlaySound(synthMusic_[idx].get());  // loop the synthesized track
    }
}

void AudioManager::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    if (!enabled_) {
        stopCurrent();
    } else {
        startCurrent();
    }
}

void AudioManager::setVolumes(float master, float music, float sfx) {
    masterVolume_ = clamp01(master);
    musicVolume_ = clamp01(music);
    sfxVolume_ = clamp01(sfx);
    if (!ready_) {
        return;
    }
    SetMasterVolume(masterVolume_);
    // Re-apply to whatever is currently playing; SFX pick it up at play time.
    if (current_ != MusicTrack::None) {
        const std::size_t idx = static_cast<std::size_t>(current_) - 1;
        if (idx < kMusicCount) {
            if (fileMusic_[idx].valid()) {
                SetMusicVolume(fileMusic_[idx].get(), musicVolume_ * fileMusicVolume_[idx]);
            } else if (synthMusic_[idx].valid()) {
                SetSoundVolume(synthMusic_[idx].get(), musicVolume_ * kSynthLevel);
            }
        }
    }
}

void AudioManager::applyManifest(const assets::AssetManifest& manifest,
                                 const std::filesystem::path& root) {
    if (!ready_) {
        return;
    }
    // Stop before unloading anything the current track might be using, then
    // restart it on whatever tier resolves afterwards (debug reload path).
    const MusicTrack playing = current_;
    stopCurrent();

    for (std::size_t i = 0; i < kSfxCount; ++i) {
        fileSfx_[i].reset();
        fileSfxVolume_[i] = 1.0f;
        const assets::AssetEntry* entry = manifest.find(kSfxIds[i]);
        if (entry == nullptr || entry->type != assets::AssetType::Sfx) {
            continue;
        }
        const Sound sound = LoadSound((root / entry->path).string().c_str());
        if (IsSoundValid(sound)) {
            fileSfx_[i] = SoundHandle(sound);
            fileSfxVolume_[i] = entry->volume;
        } else {
            log::warn("AudioManager: could not load '" + entry->path +
                      "' for role " + kSfxIds[i] + "; using synthesized fallback");
        }
    }

    for (std::size_t i = 0; i < kMusicCount; ++i) {
        fileMusic_[i].reset();
        fileMusicVolume_[i] = 1.0f;
        const assets::AssetEntry* entry = manifest.find(kMusicIds[i]);
        if (entry == nullptr || entry->type != assets::AssetType::Music) {
            continue;
        }
        Music music = LoadMusicStream((root / entry->path).string().c_str());
        if (IsMusicValid(music)) {
            music.looping = entry->loop;
            fileMusic_[i] = MusicHandle(music);
            fileMusicVolume_[i] = entry->volume;
        } else {
            log::warn("AudioManager: could not load '" + entry->path +
                      "' for role " + kMusicIds[i] + "; using synthesized fallback");
        }
    }

    current_ = MusicTrack::None;
    setMusic(playing);
}

}  // namespace cd
