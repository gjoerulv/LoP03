#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "assets/AssetManifest.hpp"
#include "audio/AudioRoles.hpp"
#include "content/LoadReport.hpp"

using cd::AmbienceTrack;
using cd::MusicTrack;
using cd::Sfx;
using cd::assets::AssetEntry;
using cd::assets::AssetManifest;
using cd::assets::AssetType;
using cd::content::LoadReport;

namespace audio = cd::audio;

TEST_CASE("audio role tables are complete, unique, and well-prefixed", "[audio]") {
    std::set<std::string> ids;
    for (const char* id : audio::kSfxIds) {
        REQUIRE(id != nullptr);
        const std::string s(id);
        CHECK(s.rfind("sfx.", 0) == 0);
        CHECK(ids.insert(s).second);
    }
    for (const char* id : audio::kMusicIds) {
        REQUIRE(id != nullptr);
        const std::string s(id);
        CHECK(s.rfind("music.", 0) == 0);
        CHECK(ids.insert(s).second);
    }
    for (const char* id : audio::kAmbienceIds) {
        REQUIRE(id != nullptr);
        const std::string s(id);
        CHECK(s.rfind("ambience.", 0) == 0);
        CHECK(ids.insert(s).second);
    }
    CHECK(ids.size() == cd::kSfxCount + cd::kMusicCount + cd::kAmbienceCount);
}

TEST_CASE("synth fallback map is in range and jingles fall to stingers", "[audio]") {
    for (std::size_t i = 0; i < cd::kMusicCount; ++i) {
        CHECK(audio::kSynthMusicIndex[i] >= -1);
        CHECK(audio::kSynthMusicIndex[i] <= 2);
    }
    const std::size_t victory = static_cast<std::size_t>(MusicTrack::Victory) - 1;
    const std::size_t defeat = static_cast<std::size_t>(MusicTrack::Defeat) - 1;
    CHECK(audio::kSynthMusicIndex[victory] == -1);
    CHECK(audio::kSynthMusicIndex[defeat] == -1);
    CHECK(audio::isJingle(MusicTrack::Victory));
    CHECK(audio::isJingle(MusicTrack::Defeat));
    CHECK_FALSE(audio::isJingle(MusicTrack::Battle));
    CHECK_FALSE(audio::isJingle(MusicTrack::None));
}

TEST_CASE("sfx rate-limit policy: first play free, spam blocked, cadence kept", "[audio]") {
    for (std::size_t i = 0; i < cd::kSfxCount; ++i) {
        CHECK(audio::kSfxMinInterval[i] >= 0.0f);
    }
    // Never played: always allowed.
    CHECK(audio::sfxAllowed(Sfx::Step, 0.0, -1.0));
    // Within the interval: blocked; at/after: allowed.
    const double step = static_cast<double>(
        audio::kSfxMinInterval[static_cast<std::size_t>(Sfx::Step)]);
    CHECK_FALSE(audio::sfxAllowed(Sfx::Step, 1.0 + step * 0.5, 1.0));
    CHECK(audio::sfxAllowed(Sfx::Step, 1.0 + step, 1.0));
    CHECK(audio::sfxAllowed(Sfx::Step, 1.0 + step * 4.0, 1.0));
}

TEST_CASE("music fade gain is monotone from 1 to 0", "[audio]") {
    CHECK(audio::fadeGain(0.0f) == 1.0f);
    CHECK(audio::fadeGain(-1.0f) == 1.0f);
    CHECK(audio::fadeGain(audio::kMusicFadeSeconds) == 0.0f);
    CHECK(audio::fadeGain(audio::kMusicFadeSeconds * 2.0f) == 0.0f);
    const float mid = audio::fadeGain(audio::kMusicFadeSeconds * 0.5f);
    CHECK(mid > 0.0f);
    CHECK(mid < 1.0f);
}

#ifdef CRYSTAL_TEST_ASSETS_DIR

namespace {

struct WavInfo {
    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint32_t dataBytes = 0;
};

std::uint32_t le32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t le16(const unsigned char* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

// Reads the canonical 44-byte RIFF/fmt/data header our generator writes.
bool wavHeader(const std::filesystem::path& file, WavInfo& out) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return false;
    }
    unsigned char h[44] = {};
    in.read(reinterpret_cast<char*>(h), sizeof(h));
    if (!in) {
        return false;
    }
    if (h[0] != 'R' || h[1] != 'I' || h[2] != 'F' || h[3] != 'F' || h[8] != 'W' ||
        h[9] != 'A' || h[10] != 'V' || h[11] != 'E' || h[36] != 'd' || h[37] != 'a' ||
        h[38] != 't' || h[39] != 'a') {
        return false;
    }
    out.format = le16(h + 20);
    out.channels = le16(h + 22);
    out.sampleRate = le32(h + 24);
    out.bitsPerSample = le16(h + 34);
    out.dataBytes = le32(h + 40);
    return true;
}

}  // namespace

TEST_CASE("shipped manifest covers every audio role with a valid entry", "[audio]") {
    const std::filesystem::path root(CRYSTAL_TEST_ASSETS_DIR);
    AssetManifest m;
    LoadReport report;
    REQUIRE(m.load(root, report));

    for (const char* id : audio::kSfxIds) {
        INFO("sfx role " << id);
        const AssetEntry* e = m.find(id);
        REQUIRE(e != nullptr);
        CHECK(e->type == AssetType::Sfx);
        CHECK(std::filesystem::exists(root / e->path));
        CHECK(e->volume > 0.0f);
        CHECK(e->volume <= 1.0f);
    }
    for (std::size_t i = 0; i < cd::kMusicCount; ++i) {
        INFO("music role " << audio::kMusicIds[i]);
        const AssetEntry* e = m.find(audio::kMusicIds[i]);
        REQUIRE(e != nullptr);
        CHECK(e->type == AssetType::Music);
        CHECK(std::filesystem::exists(root / e->path));
        CHECK(e->volume > 0.0f);
        CHECK(e->volume <= 1.0f);
        // Jingles play once; everything else loops.
        const MusicTrack track = static_cast<MusicTrack>(i + 1);
        CHECK(e->loop == !audio::isJingle(track));
    }
    for (const char* id : audio::kAmbienceIds) {
        INFO("ambience role " << id);
        const AssetEntry* e = m.find(id);
        REQUIRE(e != nullptr);
        CHECK(e->type == AssetType::Ambience);
        CHECK(std::filesystem::exists(root / e->path));
        CHECK(e->loop);
        CHECK(e->volume > 0.0f);
        CHECK(e->volume <= 0.5f);  // beds sit under the music
    }
}

TEST_CASE("every shipped audio file is a valid PCM16 mono 22050 Hz WAV", "[audio]") {
    const std::filesystem::path root =
        std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR) / "audio";
    REQUIRE(std::filesystem::exists(root));
    int files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".wav") {
            continue;
        }
        ++files;
        WavInfo info;
        INFO(entry.path().string());
        REQUIRE(wavHeader(entry.path(), info));
        CHECK(info.format == 1);  // PCM
        CHECK(info.channels == 1);
        CHECK(info.sampleRate == 22050);
        CHECK(info.bitsPerSample == 16);
        CHECK(info.dataBytes > 0);
        CHECK(info.dataBytes + 44 == std::filesystem::file_size(entry.path()));
    }
    CHECK(files == 38);  // 11 music + 6 town-ladder (M32) + 2 castle/king (M40) + 4 ambience + 15 sfx
}

TEST_CASE("every shipped audio family has a provenance record in credits.md", "[audio]") {
    const std::filesystem::path credits =
        std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR) / "credits.md";
    std::ifstream in(credits);
    REQUIRE(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();
    CHECK(text.find("audio/music/*.wav") != std::string::npos);
    CHECK(text.find("audio/ambience/*.wav") != std::string::npos);
    CHECK(text.find("audio/sfx/*.wav") != std::string::npos);
}

#endif  // CRYSTAL_TEST_ASSETS_DIR
