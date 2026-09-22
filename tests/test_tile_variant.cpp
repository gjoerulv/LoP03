#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/AssetManifest.hpp"
#include "content/LoadReport.hpp"
#include "render/TileVariant.hpp"

using cd::render::tileHash;
using cd::render::tileVariant;

namespace {

// A sequence "has period p" when every element equals the one p steps on. The
// pre-M128 accent pick, (x*31 + y*17) % n, had period n along every row — the
// diagonal stripes this hash exists to remove.
bool hasPeriod(const std::vector<int>& s, int p) {
    if (static_cast<int>(s.size()) <= p) {
        return false;
    }
    for (std::size_t i = 0; i + static_cast<std::size_t>(p) < s.size(); ++i) {
        if (s[i] != s[i + static_cast<std::size_t>(p)]) {
            return false;
        }
    }
    return true;
}

void checkNoShortPeriod(const std::vector<int>& s) {
    for (int p = 1; p <= 8; ++p) {
        INFO("period " << p);
        CHECK_FALSE(hasPeriod(s, p));
    }
}

// PNG IHDR width/height (big-endian at offsets 16 and 20), the
// test_battle_backdrop pattern.
bool pngSize(const std::filesystem::path& file, std::uint32_t& w, std::uint32_t& h) {
    std::ifstream in(file, std::ios::binary);
    unsigned char head[24] = {};
    if (!in.read(reinterpret_cast<char*>(head), sizeof(head))) {
        return false;
    }
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (int i = 0; i < 8; ++i) {
        if (head[i] != sig[i]) return false;
    }
    const auto be = [&](int at) {
        return (static_cast<std::uint32_t>(head[at]) << 24) |
               (static_cast<std::uint32_t>(head[at + 1]) << 16) |
               (static_cast<std::uint32_t>(head[at + 2]) << 8) |
               static_cast<std::uint32_t>(head[at + 3]);
    };
    w = be(16);
    h = be(20);
    return true;
}

}  // namespace

TEST_CASE("tile variant: deterministic and always in range", "[m128][render]") {
    const int weights[4] = {30, 25, 25, 20};
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 24; ++x) {
            const int a = tileVariant(0, x, y, 3, weights, 4);
            const int b = tileVariant(0, x, y, 3, weights, 4);
            CHECK(a == b);
            CHECK(a >= 0);
            CHECK(a < 4);
        }
    }
    for (int count = 1; count <= 8; ++count) {
        const std::vector<int> even(static_cast<std::size_t>(count), 1);
        for (int x = 0; x < 64; ++x) {
            const int v = tileVariant(424242u, x, x * 3, 5, even.data(), count);
            CHECK(v >= 0);
            CHECK(v < count);
        }
    }
    CHECK(tileHash(1, 2, 3, 4) == tileHash(1, 2, 3, 4));
    CHECK(tileHash(1, 2, 3, 4) != tileHash(1, 3, 2, 4));  // x and y are not interchangeable
}

TEST_CASE("tile variant: degenerate tables pick the first variant", "[m128][render]") {
    const int only[4] = {100, 0, 0, 0};
    const int none[3] = {0, 0, 0};
    const int negative[2] = {-5, 10};
    for (int i = 0; i < 200; ++i) {
        CHECK(tileVariant(9, i, i / 7, 1, only, 4) == 0);
        CHECK(tileVariant(9, i, i / 7, 1, none, 3) == 0);
        CHECK(tileVariant(9, i, i / 7, 1, negative, 2) == 1);  // a negative weight counts as 0
    }
    CHECK(tileVariant(9, 1, 1, 1, nullptr, 4) == 0);
    CHECK(tileVariant(9, 1, 1, 1, only, 0) == 0);
}

TEST_CASE("tile variant: the salt and the seed each change the pattern", "[m128][render]") {
    const int weights[4] = {30, 25, 25, 20};
    int saltDiffers = 0;
    int seedDiffers = 0;
    const int cells = 24 * 12;
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 24; ++x) {
            if (tileVariant(0, x, y, 1, weights, 4) != tileVariant(0, x, y, 2, weights, 4)) {
                ++saltDiffers;
            }
            if (tileVariant(1, x, y, 7, weights, 4) != tileVariant(2, x, y, 7, weights, 4)) {
                ++seedDiffers;
            }
        }
    }
    // Two independent picks agree about a quarter of the time with these
    // weights; identical patterns would be a hash that ignores its input.
    CHECK(saltDiffers > cells / 2);
    CHECK(seedDiffers > cells / 2);
}

TEST_CASE("tile variant: the weights hold over a large field", "[m128][render]") {
    const int weights[4] = {55, 20, 15, 10};
    int counts[4] = {0, 0, 0, 0};
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) {
            ++counts[tileVariant(77, x, y, 3, weights, 4)];
        }
    }
    for (int i = 0; i < 4; ++i) {
        INFO("bucket " << i << " got " << counts[i]);
        CHECK(counts[i] > weights[i] * 100 - 300);  // within 3 % of 10 000 cells
        CHECK(counts[i] < weights[i] * 100 + 300);
    }
}

TEST_CASE("tile variant: no short period along rows, columns or diagonals", "[m128][render]") {
    const int weights[4] = {30, 25, 25, 20};
    // The town ring (every town's salt) and a 100-cell field.
    for (int town = 1; town <= 7; ++town) {
        const auto salt = static_cast<std::uint64_t>(town);
        std::vector<int> top;
        std::vector<int> bottom;
        std::vector<int> left;
        std::vector<int> right;
        for (int x = 0; x < 24; ++x) {
            top.push_back(tileVariant(0, x, 0, salt, weights, 4));
            bottom.push_back(tileVariant(0, x, 11, salt, weights, 4));
        }
        for (int y = 0; y < 12; ++y) {
            left.push_back(tileVariant(0, 0, y, salt, weights, 4));
            right.push_back(tileVariant(0, 23, y, salt, weights, 4));
        }
        INFO("town " << town);
        checkNoShortPeriod(top);
        checkNoShortPeriod(bottom);
        checkNoShortPeriod(left);
        checkNoShortPeriod(right);
    }
    const int wall[4] = {55, 20, 15, 10};
    for (int line = 0; line < 100; line += 9) {
        std::vector<int> row;
        std::vector<int> col;
        std::vector<int> diag;
        std::vector<int> anti;
        for (int i = 0; i < 100; ++i) {
            row.push_back(tileVariant(424242u, i, line, 11, wall, 4));
            col.push_back(tileVariant(424242u, line, i, 11, wall, 4));
            diag.push_back(tileVariant(424242u, i, (i + line) % 100, 11, wall, 4));
            anti.push_back(tileVariant(424242u, i, (line + 100 - i) % 100, 11, wall, 4));
        }
        INFO("line " << line);
        checkNoShortPeriod(row);
        checkNoShortPeriod(col);
        checkNoShortPeriod(diag);
        checkNoShortPeriod(anti);
    }
}

TEST_CASE("tile variant: every shipped M128 tile is a 16x16 texture", "[m128][lint]") {
    cd::assets::AssetManifest m;
    cd::content::LoadReport report;
    REQUIRE(m.load(std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR), report));
    std::vector<std::string> ids;
    for (int town = 1; town <= 7; ++town) {
        for (const char* kind : {"tree", "ground"}) {
            for (int v = 1; v <= 4; ++v) {
                ids.push_back("tiles.town." + std::to_string(town) + "." + kind + "." +
                              std::to_string(v));
            }
        }
    }
    for (const char* theme : {"ruined_keep", "crystal_mine", "hollow_forest", "goosy_gauntlet"}) {
        for (int v = 1; v <= 4; ++v) {
            ids.push_back(std::string("tiles.") + theme + ".wall." + std::to_string(v));
        }
    }
    CHECK(ids.size() == 72);
    for (const std::string& id : ids) {
        INFO(id);
        const cd::assets::AssetEntry* e = m.find(id);
        REQUIRE(e != nullptr);
        CHECK(e->type == cd::assets::AssetType::Texture);
        std::uint32_t w = 0;
        std::uint32_t h = 0;
        REQUIRE(pngSize(std::filesystem::path(CRYSTAL_TEST_ASSETS_DIR) / e->path, w, h));
        CHECK(w == 16u);
        CHECK(h == 16u);
    }
}
