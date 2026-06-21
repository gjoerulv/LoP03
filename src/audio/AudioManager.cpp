#include "audio/AudioManager.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#include "raylib.h"

namespace cd {

namespace {

constexpr int kSampleRate = 22050;
constexpr float kPi = 3.14159265358979323846f;

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
    SetMasterVolume(0.5f);

    sfx_[static_cast<std::size_t>(Sfx::Move)] = makeTone(880.0f, 0.04f, 0.3f);
    sfx_[static_cast<std::size_t>(Sfx::Confirm)] = makeTone(660.0f, 0.09f, 0.4f, false, 990.0f);
    sfx_[static_cast<std::size_t>(Sfx::Cancel)] = makeTone(440.0f, 0.09f, 0.35f, false, 300.0f);
    sfx_[static_cast<std::size_t>(Sfx::Hit)] = makeTone(0.0f, 0.10f, 0.4f, true);
    sfx_[static_cast<std::size_t>(Sfx::Heal)] = makeTone(523.0f, 0.18f, 0.35f, false, 784.0f);
    sfx_[static_cast<std::size_t>(Sfx::Ko)] = makeTone(330.0f, 0.25f, 0.4f, false, 120.0f);
    sfx_[static_cast<std::size_t>(Sfx::Victory)] = makeTone(523.0f, 0.35f, 0.45f, false, 1046.0f);
    sfx_[static_cast<std::size_t>(Sfx::Defeat)] = makeTone(220.0f, 0.45f, 0.4f, false, 90.0f);
    sfx_[static_cast<std::size_t>(Sfx::Chest)] = makeTone(784.0f, 0.12f, 0.4f, false, 1175.0f);

    // ~2s looping placeholder tracks.
    music_[0] = makeLoop({523.0f, 659.0f, 784.0f, 659.0f}, 0.22f, 2, 0.12f);  // Town (major)
    music_[1] = makeLoop({392.0f, 466.0f, 523.0f, 466.0f}, 0.28f, 2, 0.12f);  // Dungeon (minor)
    music_[2] = makeLoop({440.0f, 554.0f, 659.0f, 554.0f}, 0.16f, 3, 0.13f);  // Battle (tense)
}

void AudioManager::play(Sfx id) {
    if (!ready_ || !enabled_) {
        return;
    }
    const SoundHandle& s = sfx_[static_cast<std::size_t>(id)];
    if (s.valid()) {
        PlaySound(s.get());
    }
}

void AudioManager::setMusic(MusicTrack track) {
    if (track == current_) {
        return;
    }
    if (ready_ && current_ != MusicTrack::None) {
        const std::size_t prev = static_cast<std::size_t>(current_) - 1;
        if (prev < music_.size() && music_[prev].valid()) {
            StopSound(music_[prev].get());
        }
    }
    current_ = track;
    if (ready_ && enabled_ && current_ != MusicTrack::None) {
        const std::size_t idx = static_cast<std::size_t>(current_) - 1;
        if (idx < music_.size() && music_[idx].valid()) {
            PlaySound(music_[idx].get());
        }
    }
}

void AudioManager::update() {
    if (!ready_ || !enabled_ || current_ == MusicTrack::None) {
        return;
    }
    const std::size_t idx = static_cast<std::size_t>(current_) - 1;
    if (idx < music_.size() && music_[idx].valid() && !IsSoundPlaying(music_[idx].get())) {
        PlaySound(music_[idx].get());  // loop the track
    }
}

void AudioManager::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_ && ready_ && current_ != MusicTrack::None) {
        const std::size_t idx = static_cast<std::size_t>(current_) - 1;
        if (idx < music_.size() && music_[idx].valid()) {
            StopSound(music_[idx].get());
        }
    }
}

}  // namespace cd
