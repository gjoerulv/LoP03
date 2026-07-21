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

std::size_t musicIndex(MusicTrack track) {
    return static_cast<std::size_t>(track) - 1;  // None excluded from tables
}

std::size_t ambienceIndex(AmbienceTrack track) {
    return static_cast<std::size_t>(track) - 1;  // None excluded from tables
}

}  // namespace

AudioManager::DeviceGuard::DeviceGuard() { InitAudioDevice(); }
AudioManager::DeviceGuard::~DeviceGuard() {
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
}

AudioManager::AudioManager() {
    lastSfxTime_.fill(-1.0);
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
    synthSfx_[static_cast<std::size_t>(Sfx::Error)] =
        makeTone(220.0f, 0.12f, 0.35f, false, 180.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Hit)] = makeTone(0.0f, 0.10f, 0.4f, true);
    synthSfx_[static_cast<std::size_t>(Sfx::HitMagic)] =
        makeTone(1200.0f, 0.12f, 0.35f, false, 400.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Heal)] =
        makeTone(523.0f, 0.18f, 0.35f, false, 784.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Status)] =
        makeTone(587.0f, 0.10f, 0.3f, false, 880.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Ko)] = makeTone(330.0f, 0.25f, 0.4f, false, 120.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Victory)] =
        makeTone(523.0f, 0.35f, 0.45f, false, 1046.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Defeat)] =
        makeTone(220.0f, 0.45f, 0.4f, false, 90.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Chest)] =
        makeTone(784.0f, 0.12f, 0.4f, false, 1175.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Step)] = makeTone(0.0f, 0.03f, 0.15f, true);
    synthSfx_[static_cast<std::size_t>(Sfx::Door)] =
        makeTone(392.0f, 0.10f, 0.3f, false, 262.0f);
    synthSfx_[static_cast<std::size_t>(Sfx::Interact)] =
        makeTone(659.0f, 0.10f, 0.35f, false, 880.0f);

    // ~2s looping placeholder tracks (0 town, 1 dungeon, 2 battle).
    synthMusic_[0] = makeLoop({523.0f, 659.0f, 784.0f, 659.0f}, 0.22f, 2, 0.12f);
    synthMusic_[1] = makeLoop({392.0f, 466.0f, 523.0f, 466.0f}, 0.28f, 2, 0.12f);
    synthMusic_[2] = makeLoop({440.0f, 554.0f, 659.0f, 554.0f}, 0.16f, 3, 0.13f);
}

void AudioManager::play(Sfx id) {
    if (!ready_ || !enabled_) {
        return;
    }
    const std::size_t i = static_cast<std::size_t>(id);
    const double now = GetTime();
    if (!audio::sfxAllowed(id, now, lastSfxTime_[i])) {
        return;
    }
    if (fileSfx_[i].valid()) {
        lastSfxTime_[i] = now;
        SetSoundVolume(fileSfx_[i].get(), sfxVolume_ * fileSfxVolume_[i]);
        PlaySound(fileSfx_[i].get());
        return;
    }
    if (synthSfx_[i].valid()) {
        lastSfxTime_[i] = now;
        SetSoundVolume(synthSfx_[i].get(), sfxVolume_ * kSynthLevel);
        PlaySound(synthSfx_[i].get());
    }
    // Neither tier available: silence (still no crash).
}

void AudioManager::cancelFade() {
    if (fadingIndex_ < 0) {
        return;
    }
    const std::size_t idx = static_cast<std::size_t>(fadingIndex_);
    if (idx < kMusicCount && fileMusic_[idx].valid()) {
        StopMusicStream(fileMusic_[idx].get());
    }
    fadingIndex_ = -1;
    fadeElapsed_ = 0.0f;
}

void AudioManager::stopCurrent() {
    if (!ready_ || current_ == MusicTrack::None) {
        return;
    }
    const std::size_t idx = musicIndex(current_);
    if (idx >= kMusicCount) {
        return;
    }
    if (fileMusic_[idx].valid()) {
        StopMusicStream(fileMusic_[idx].get());
        return;
    }
    const int synth = audio::kSynthMusicIndex[idx];
    if (synth >= 0 && synthMusic_[static_cast<std::size_t>(synth)].valid()) {
        StopSound(synthMusic_[static_cast<std::size_t>(synth)].get());
    }
}

void AudioManager::startCurrent() {
    if (!ready_ || !enabled_ || current_ == MusicTrack::None) {
        return;
    }
    const std::size_t idx = musicIndex(current_);
    if (idx >= kMusicCount) {
        return;
    }
    if (fileMusic_[idx].valid()) {
        SetMusicVolume(fileMusic_[idx].get(), musicVolume_ * fileMusicVolume_[idx]);
        PlayMusicStream(fileMusic_[idx].get());
        return;
    }
    if (audio::isJingle(current_)) {
        // No jingle file: play the matching stinger SFX so battle end is
        // never silent, and leave the music channel idle.
        play(current_ == MusicTrack::Victory ? Sfx::Victory : Sfx::Defeat);
        current_ = MusicTrack::None;
        return;
    }
    const int synth = audio::kSynthMusicIndex[idx];
    if (synth >= 0 && synthMusic_[static_cast<std::size_t>(synth)].valid()) {
        SetSoundVolume(synthMusic_[static_cast<std::size_t>(synth)].get(),
                       musicVolume_ * kSynthLevel);
        PlaySound(synthMusic_[static_cast<std::size_t>(synth)].get());
    }
}

void AudioManager::setMusic(MusicTrack track) {
    if (track == current_) {
        return;
    }
    if (!ready_) {
        current_ = track;
        return;
    }
    // Start a short fade for an outgoing file stream; hard-cut everything
    // else (synth loops are quiet blips, and only one fade runs at a time).
    if (current_ != MusicTrack::None) {
        const std::size_t idx = musicIndex(current_);
        if (idx < kMusicCount && fileMusic_[idx].valid() && enabled_) {
            cancelFade();
            fadingIndex_ = static_cast<int>(idx);
            fadeElapsed_ = 0.0f;
        } else {
            stopCurrent();
        }
    }
    current_ = track;
    if (current_ != MusicTrack::None) {
        const std::size_t idx = musicIndex(current_);
        if (static_cast<int>(idx) == fadingIndex_) {
            cancelFade();  // switching back mid-fade: restart the stream clean
        }
    }
    startCurrent();
}

void AudioManager::loadTownMusicSlot() {
    if (!ready_) {
        return;
    }
    const std::size_t townIdx = musicIndex(MusicTrack::Town);
    if (townIdx >= kMusicCount) {
        return;
    }
    fileMusic_[townIdx].reset();
    fileMusicVolume_[townIdx] = 1.0f;
    const std::size_t v = static_cast<std::size_t>(townMusic_ - 1);
    if (v >= kTownMusicVariants || townMusicRel_[v].empty()) {
        return;  // no file resolves: synth fallback (kSynthMusicIndex[Town] = 0)
    }
    Music music = LoadMusicStream((assetsRoot_ / townMusicRel_[v]).string().c_str());
    if (IsMusicValid(music)) {
        music.looping = true;
        fileMusic_[townIdx] = MusicHandle(music);
        fileMusicVolume_[townIdx] = townMusicVol_[v];
    }
}

void AudioManager::setTown(int town) {
    const int maxTown = static_cast<int>(kTownMusicVariants);
    const int t = town < 1 ? 1 : (town > maxTown ? maxTown : town);
    if (t == townMusic_) {
        return;
    }
    const bool wasPlaying = ready_ && enabled_ && current_ == MusicTrack::Town;
    if (ready_ && current_ == MusicTrack::Town) {
        cancelFade();
        stopCurrent();
    }
    townMusic_ = t;
    loadTownMusicSlot();
    if (wasPlaying) {
        startCurrent();  // restart Town music on the new variant stream
    }
}

void AudioManager::stopAmbience() {
    if (!ready_ || currentAmbience_ == AmbienceTrack::None) {
        return;
    }
    const std::size_t idx = ambienceIndex(currentAmbience_);
    if (idx < kAmbienceCount && fileAmbience_[idx].valid()) {
        StopMusicStream(fileAmbience_[idx].get());
    }
}

void AudioManager::startAmbience() {
    if (!ready_ || !enabled_ || currentAmbience_ == AmbienceTrack::None) {
        return;
    }
    const std::size_t idx = ambienceIndex(currentAmbience_);
    if (idx < kAmbienceCount && fileAmbience_[idx].valid()) {
        // Ambience follows the SFX slider (M27 owner decision), not Music.
        SetMusicVolume(fileAmbience_[idx].get(), sfxVolume_ * fileAmbienceVolume_[idx]);
        PlayMusicStream(fileAmbience_[idx].get());
    }
    // No synthesized ambience tier: missing files mean silence.
}

void AudioManager::setAmbience(AmbienceTrack track) {
    if (track == currentAmbience_) {
        return;
    }
    stopAmbience();
    currentAmbience_ = track;
    startAmbience();
}

void AudioManager::update() {
    if (!ready_) {
        return;
    }
    if (fadingIndex_ >= 0) {
        const std::size_t idx = static_cast<std::size_t>(fadingIndex_);
        if (idx < kMusicCount && fileMusic_[idx].valid()) {
            fadeElapsed_ += GetFrameTime();
            const float gain = audio::fadeGain(fadeElapsed_);
            if (gain <= 0.0f) {
                cancelFade();
            } else {
                SetMusicVolume(fileMusic_[idx].get(),
                               gain * musicVolume_ * fileMusicVolume_[idx]);
                UpdateMusicStream(fileMusic_[idx].get());
            }
        } else {
            fadingIndex_ = -1;
        }
    }
    if (enabled_ && currentAmbience_ != AmbienceTrack::None) {
        const std::size_t idx = ambienceIndex(currentAmbience_);
        if (idx < kAmbienceCount && fileAmbience_[idx].valid()) {
            UpdateMusicStream(fileAmbience_[idx].get());
        }
    }
    if (!enabled_ || current_ == MusicTrack::None) {
        return;
    }
    const std::size_t idx = musicIndex(current_);
    if (idx >= kMusicCount) {
        return;
    }
    if (fileMusic_[idx].valid()) {
        if (static_cast<int>(idx) == fadingIndex_) {
            return;  // already updated above while fading out
        }
        UpdateMusicStream(fileMusic_[idx].get());  // looping handled by the stream
        // One-shot jingles end on their own; free the channel when done.
        if (!fileMusic_[idx].get().looping && !IsMusicStreamPlaying(fileMusic_[idx].get())) {
            current_ = MusicTrack::None;
        }
        return;
    }
    const int synth = audio::kSynthMusicIndex[idx];
    if (synth >= 0 && synthMusic_[static_cast<std::size_t>(synth)].valid() &&
        !IsSoundPlaying(synthMusic_[static_cast<std::size_t>(synth)].get())) {
        PlaySound(synthMusic_[static_cast<std::size_t>(synth)].get());  // loop the synth track
    }
}

void AudioManager::setEnabled(bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    if (!enabled_) {
        cancelFade();
        stopCurrent();
        stopAmbience();
    } else {
        startCurrent();
        startAmbience();
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
        const std::size_t idx = musicIndex(current_);
        if (idx < kMusicCount) {
            if (fileMusic_[idx].valid()) {
                SetMusicVolume(fileMusic_[idx].get(), musicVolume_ * fileMusicVolume_[idx]);
            } else {
                const int synth = audio::kSynthMusicIndex[idx];
                if (synth >= 0 && synthMusic_[static_cast<std::size_t>(synth)].valid()) {
                    SetSoundVolume(synthMusic_[static_cast<std::size_t>(synth)].get(),
                                   musicVolume_ * kSynthLevel);
                }
            }
        }
    }
    if (currentAmbience_ != AmbienceTrack::None) {
        const std::size_t idx = ambienceIndex(currentAmbience_);
        if (idx < kAmbienceCount && fileAmbience_[idx].valid()) {
            // Ambience follows the SFX slider (M27 owner decision), not Music.
        SetMusicVolume(fileAmbience_[idx].get(), sfxVolume_ * fileAmbienceVolume_[idx]);
        }
    }
}

void AudioManager::applyManifest(const assets::AssetManifest& manifest,
                                 const std::filesystem::path& root) {
    if (!ready_) {
        return;
    }
    // Stop before unloading anything the current tracks might be using, then
    // restart them on whatever tier resolves afterwards (debug reload path).
    const MusicTrack playing = current_;
    const AmbienceTrack playingAmbience = currentAmbience_;
    cancelFade();
    stopCurrent();
    stopAmbience();

    for (std::size_t i = 0; i < kSfxCount; ++i) {
        fileSfx_[i].reset();
        fileSfxVolume_[i] = 1.0f;
        const assets::AssetEntry* entry = manifest.find(audio::kSfxIds[i]);
        if (entry == nullptr || entry->type != assets::AssetType::Sfx) {
            continue;
        }
        const Sound sound = LoadSound((root / entry->path).string().c_str());
        if (IsSoundValid(sound)) {
            fileSfx_[i] = SoundHandle(sound);
            fileSfxVolume_[i] = entry->volume;
        } else {
            log::warn("AudioManager: could not load '" + entry->path + "' for role " +
                      audio::kSfxIds[i] + "; using synthesized fallback");
        }
    }

    for (std::size_t i = 0; i < kMusicCount; ++i) {
        fileMusic_[i].reset();
        fileMusicVolume_[i] = 1.0f;
        const assets::AssetEntry* entry = manifest.find(audio::kMusicIds[i]);
        if (entry == nullptr || entry->type != assets::AssetType::Music) {
            continue;
        }
        Music music = LoadMusicStream((root / entry->path).string().c_str());
        if (IsMusicValid(music)) {
            music.looping = entry->loop;
            fileMusic_[i] = MusicHandle(music);
            fileMusicVolume_[i] = entry->volume;
        } else {
            log::warn("AudioManager: could not load '" + entry->path + "' for role " +
                      audio::kMusicIds[i] + "; using synthesized fallback");
        }
    }

    for (std::size_t i = 0; i < kAmbienceCount; ++i) {
        fileAmbience_[i].reset();
        fileAmbienceVolume_[i] = 1.0f;
        const assets::AssetEntry* entry = manifest.find(audio::kAmbienceIds[i]);
        if (entry == nullptr || entry->type != assets::AssetType::Ambience) {
            continue;
        }
        Music music = LoadMusicStream((root / entry->path).string().c_str());
        if (IsMusicValid(music)) {
            music.looping = true;  // ambience always loops
            fileAmbience_[i] = MusicHandle(music);
            fileAmbienceVolume_[i] = entry->volume;
        } else {
            log::warn("AudioManager: could not load '" + entry->path + "' for role " +
                      audio::kAmbienceIds[i] + "; ambience stays silent");
        }
    }

    // M32 town-music variants: town 1 = base `music.town` (already loaded above);
    // towns 2..7 = `music.town.<n>` when present, else fall back to the base
    // path. Stored as relative paths so a town switch can (re)load one stream.
    assetsRoot_ = root;
    const assets::AssetEntry* base = manifest.find("music.town");
    const bool baseOk = base != nullptr && base->type == assets::AssetType::Music;
    const std::string basePath = baseOk ? base->path : std::string();
    const float baseVol = baseOk ? base->volume : 1.0f;
    for (std::size_t t = 0; t < kTownMusicVariants; ++t) {
        townMusicRel_[t] = basePath;
        townMusicVol_[t] = baseVol;
        if (t == 0) {
            continue;  // town 1 is the base track
        }
        const std::string id = "music.town." + std::to_string(t + 1);
        const assets::AssetEntry* e = manifest.find(id);
        if (e != nullptr && e->type == assets::AssetType::Music) {
            townMusicRel_[t] = e->path;
            townMusicVol_[t] = e->volume;
        }
    }
    if (townMusic_ != 1) {
        loadTownMusicSlot();  // rebind the Town slot to the active town's variant
    }

    current_ = MusicTrack::None;
    setMusic(playing);
    currentAmbience_ = AmbienceTrack::None;
    setAmbience(playingAmbience);
}

}  // namespace cd
