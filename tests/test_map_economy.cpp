// M83 — the 4-floor map economy. Proves: the owner's chance table (15..75%
// by town, +12 points each), the committed pure-hash roll (reload-proof, and
// statistically near its stated chance), the shared grant rule (the fourth
// piece fires the reveal), the IOU bank (cap 3, never overpaying the pouch,
// never silently losing an IOU at payout), and the save round-trip with a
// tamper clamp.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "game/Party.hpp"
#include "game/TreasureMap.hpp"
#include "save/SaveSystem.hpp"

using namespace cd;
namespace fs = std::filesystem;

namespace {

const content::ContentDatabase& db() {
    static content::ContentDatabase database = [] {
        content::ContentDatabase d;
        content::LoadReport report;
        REQUIRE(content::loadAll(fs::path(CRYSTAL_TEST_DATA_DIR), d, report));
        return d;
    }();
    return database;
}

fs::path makeTempDir() {
    static std::atomic<int> counter{0};
    std::random_device rd;
    fs::path dir = fs::temp_directory_path() /
                   ("cd_mapecon_" + std::to_string(rd()) + "_" + std::to_string(counter++));
    fs::create_directories(dir);
    return dir;
}

}  // namespace

TEST_CASE("mapdrop: the chance table is the owner's, exactly", "[mapdrop]") {
    CHECK(mapDropChancePct(1) == 0);  // town 1 never rolls
    CHECK(mapDropChancePct(2) == 15);
    CHECK(mapDropChancePct(3) == 27);
    CHECK(mapDropChancePct(4) == 39);
    CHECK(mapDropChancePct(5) == 51);
    CHECK(mapDropChancePct(6) == 63);
    CHECK(mapDropChancePct(7) == 75);
    // The M84 perk hook adds points on top; the base itself caps at 75.
    CHECK(mapDropChancePct(2, 5) == 20);
    CHECK(mapDropChancePct(7, 5) == 80);
    CHECK(mapDropChancePct(7, 100) == 100);  // clamped, never above certain
    CHECK(mapDropChancePct(1, 5) == 0);      // no bonus can open town 1
}

TEST_CASE("mapdrop: the roll is committed and statistically honest", "[mapdrop]") {
    // Committed: the same run seed always answers the same way (the
    // reload-proof property — result-time has no RNG to reroll).
    for (std::uint64_t seed : {1ull, 424242ull, 999983ull}) {
        CHECK(mapDropRolls(seed, 5) == mapDropRolls(seed, 5));
        CHECK_FALSE(mapDropRolls(seed, 1));  // town 1 never
    }
    // Statistically honest: over many seeds the hit rate sits near the table
    // (a wide +/-4 point band keeps this deterministic test un-flaky — the
    // seeds are fixed, so this can only break if the hash or table changes).
    for (int town : {2, 7}) {
        int hits = 0;
        constexpr int kSeeds = 4000;
        for (int i = 0; i < kSeeds; ++i) {
            if (mapDropRolls(static_cast<std::uint64_t>(i) * 2654435761u + 17, town)) {
                ++hits;
            }
        }
        const int pct = hits * 100 / kSeeds;
        INFO("town " << town << " observed " << pct << "%");
        CHECK(pct >= mapDropChancePct(town) - 4);
        CHECK(pct <= mapDropChancePct(town) + 4);
    }
}

TEST_CASE("mapdrop: the fourth granted piece fires the reveal", "[mapdrop]") {
    int pieces = 0;
    TreasureReveal treasure;
    for (int i = 0; i < kMapPiecesNeeded - 1; ++i) {
        CHECK_FALSE(grantMapPiece(pieces, treasure, 5, db(), 424242, 260));
        CHECK_FALSE(treasure.active);
    }
    CHECK(pieces == 3);
    CHECK(grantMapPiece(pieces, treasure, 5, db(), 424242, 260));
    CHECK(pieces == 0);  // the pouch converts into the reveal
    CHECK(treasure.active);
    CHECK(treasure.town == 5);
    CHECK(treasure.scalePct == 260);
    CHECK(db().findBoss(treasure.bossId) != nullptr);  // a real roster guard
    CHECK(treasure.bossId == treasureGuardBossId(db(), 424242, 5));  // seeded, stable (M106: town-gated)
}

TEST_CASE("mapdrop: the IOU bank caps at three and pays without losses", "[mapdrop]") {
    int owed = 0;
    CHECK(bankMapDebt(owed));
    CHECK(bankMapDebt(owed));
    CHECK(bankMapDebt(owed));
    CHECK(owed == kMapPiecesOwedMax);
    CHECK_FALSE(bankMapDebt(owed));  // the ledger is full; that roll is lost
    CHECK(owed == kMapPiecesOwedMax);

    // Payout with an empty pouch: everything fits.
    int pieces = 0;
    CHECK(payMapDebt(pieces, owed) == 3);
    CHECK(pieces == 3);
    CHECK(owed == 0);

    // Payout that would overfill: pays what fits, KEEPS the rest banked (a
    // fourth piece needs a run's context to fire a reveal, so the pouch caps
    // at 3 — and an IOU is never silently lost).
    pieces = 2;
    owed = 3;
    CHECK(payMapDebt(pieces, owed) == 1);
    CHECK(pieces == 3);
    CHECK(owed == 2);

    // A full pouch pays nothing and the debt stands whole.
    pieces = 3;
    CHECK(payMapDebt(pieces, owed) == 0);
    CHECK(pieces == 3);
    CHECK(owed == 2);
}

TEST_CASE("mapdrop: the owed bank round-trips the save and clamps tampering", "[mapdrop]") {
    const fs::path dir = makeTempDir();
    const save::SaveSystem saves(db(), dir);

    Party p;
    p.members.push_back(createCharacter(*db().findClass("knight"), "Rolan"));
    p.mapPieces = 2;
    p.mapPiecesOwed = 2;

    content::LoadReport rep;
    REQUIRE(saves.save(save::SaveSlot::Manual1, p, rep));
    Party loaded;
    content::LoadReport rep2;
    REQUIRE(saves.load(save::SaveSlot::Manual1, loaded, rep2));
    CHECK(loaded.mapPieces == 2);
    CHECK(loaded.mapPiecesOwed == 2);

    // Tampering above the cap degrades to the cap — never a crash, never an
    // overpaying ledger. (Patch the saved JSON's field in place.)
    const fs::path file = saves.slotPath(save::SaveSlot::Manual1);
    std::string text;
    {
        std::ifstream in(file, std::ios::binary);
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const std::string needle = "\"mapPiecesOwed\": 2";
    const std::size_t at = text.find(needle);
    REQUIRE(at != std::string::npos);
    text.replace(at, needle.size(), "\"mapPiecesOwed\": 9");
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << text;
    }
    Party tampered;
    content::LoadReport rep3;
    REQUIRE(saves.load(save::SaveSlot::Manual1, tampered, rep3));
    CHECK(tampered.mapPiecesOwed == kMapPiecesOwedMax);

    fs::remove_all(dir);
}
