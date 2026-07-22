#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/Definitions.hpp"
#include "content/LoadReport.hpp"
#include "game/BossDrops.hpp"

// M39 boss legendary & token drops: the pure, deterministic drop rules (seeded
// reload-proof rolls, the town+depth ramp, the owner-fixed caps and town-7 double
// tokens) and the shared legendary pool. The party award + result UI are
// integration, covered by the manual matrix and the capture scene.

using namespace cd;

TEST_CASE("boss drops: the town/depth eligibility gate is town>=3 AND depth>=4",
          "[bossdrops]") {
    CHECK(bossDropEligible(3, 4));
    CHECK(bossDropEligible(7, 20));
    CHECK_FALSE(bossDropEligible(2, 4));   // town too low
    CHECK_FALSE(bossDropEligible(3, 3));   // depth too low
    CHECK_FALSE(bossDropEligible(1, 99));  // town 1 never drops
    CHECK_FALSE(bossDropEligible(9, 1));   // shallow never drops
}

TEST_CASE("boss drops: chance ramps from the t3/d4 floor to the t7/d20 caps",
          "[bossdrops]") {
    // Owner-fixed anchors.
    CHECK(bossTokenChancePct(3, 4) == 15);
    CHECK(bossLegendaryChancePct(3, 4) == 5);
    CHECK(bossTokenChancePct(7, 20) == 75);      // token cap
    CHECK(bossLegendaryChancePct(7, 20) == 30);  // legendary cap

    // A mid point on the combined ramp (town 5, depth 12 -> progress 500/1000).
    CHECK(bossTokenChancePct(5, 12) == 45);
    CHECK(bossLegendaryChancePct(5, 12) == 17);

    // Beyond the anchors the caps hold (town clamps at 7, depth past 20 too).
    CHECK(bossTokenChancePct(9, 40) == 75);
    CHECK(bossLegendaryChancePct(9, 40) == 30);
    CHECK(bossTokenChancePct(7, 200) == 75);
}

TEST_CASE("boss drops: the ramp is monotonic non-decreasing in town and depth",
          "[bossdrops]") {
    for (int town = 3; town <= 7; ++town) {
        for (int depth = 4; depth <= 20; ++depth) {
            if (depth + 1 <= 20) {
                CHECK(bossTokenChancePct(town, depth + 1) >= bossTokenChancePct(town, depth));
                CHECK(bossLegendaryChancePct(town, depth + 1) >=
                      bossLegendaryChancePct(town, depth));
            }
            if (town + 1 <= 7) {
                CHECK(bossTokenChancePct(town + 1, depth) >= bossTokenChancePct(town, depth));
                CHECK(bossLegendaryChancePct(town + 1, depth) >=
                      bossLegendaryChancePct(town, depth));
            }
        }
    }
}

TEST_CASE("boss drops: a token drop pays 2 in town 7 and 1 elsewhere", "[bossdrops]") {
    CHECK(bossTokenCount(7) == 2);
    CHECK(bossTokenCount(6) == 1);
    CHECK(bossTokenCount(3) == 1);
}

TEST_CASE("boss drops: the rolls are deterministic (reload-proof)", "[bossdrops]") {
    for (std::uint64_t s : {1ull, 42ull, 999999ull, 18446744073709551615ull}) {
        CHECK(bossTokenRolls(s, 5, 12) == bossTokenRolls(s, 5, 12));
        CHECK(bossLegendaryRolls(s, 5, 12) == bossLegendaryRolls(s, 5, 12));
        const int idx = bossLegendaryIndex(s, 8);
        CHECK(idx >= 0);
        CHECK(idx < 8);
        CHECK(bossLegendaryIndex(s, 8) == bossLegendaryIndex(s, 8));
    }
    CHECK(bossLegendaryIndex(7u, 0) == 0);  // empty-pool guard
}

TEST_CASE("boss drops: over many seeds the rates match the caps at t7/d20",
          "[bossdrops]") {
    int tokenHits = 0;
    int legendaryHits = 0;
    int bothHits = 0;
    const int n = 4000;
    for (std::uint64_t s = 1; s <= static_cast<std::uint64_t>(n); ++s) {
        const std::uint64_t seed = s * 2654435761u;
        const bool tok = bossTokenRolls(seed, 7, 20);
        const bool leg = bossLegendaryRolls(seed, 7, 20);
        tokenHits += tok ? 1 : 0;
        legendaryHits += leg ? 1 : 0;
        bothHits += (tok && leg) ? 1 : 0;
    }
    // 75% of 4000 = 3000; 30% = 1200; independent joint 22.5% = 900. Generous bands.
    CHECK(tokenHits > 2800);
    CHECK(tokenHits < 3200);
    CHECK(legendaryHits > 1050);
    CHECK(legendaryHits < 1350);
    // The two rolls use distinct salts, so the joint frequency tracks the product
    // of the marginals (independence), not either marginal.
    CHECK(bothHits > 750);
    CHECK(bothHits < 1050);
}

TEST_CASE("boss drops: at the t3/d4 floor the rates are genuinely low", "[bossdrops]") {
    int tokenHits = 0;
    int legendaryHits = 0;
    const int n = 4000;
    for (std::uint64_t s = 1; s <= static_cast<std::uint64_t>(n); ++s) {
        const std::uint64_t seed = s * 2654435761u;
        tokenHits += bossTokenRolls(seed, 3, 4) ? 1 : 0;
        legendaryHits += bossLegendaryRolls(seed, 3, 4) ? 1 : 0;
    }
    // 15% of 4000 = 600; 5% = 200. Generous bands.
    CHECK(tokenHits > 450);
    CHECK(tokenHits < 780);
    CHECK(legendaryHits > 120);
    CHECK(legendaryHits < 300);
}

TEST_CASE("boss drops: the shared legendary pool is the sorted legendary roster",
          "[bossdrops]") {
    content::ContentDatabase db;
    content::LoadReport rep;
    REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep));

    const std::vector<std::string> pool = legendaryDropPool(db);
    CHECK(pool.size() >= 5);  // M34 legendaries + M37 legendary weapons
    CHECK(std::is_sorted(pool.begin(), pool.end()));
    for (const std::string& id : pool) {
        const content::ItemDef* it = db.findItem(id);
        REQUIRE(it != nullptr);
        CHECK(it->rarity == content::Rarity::Legendary);
        CHECK((it->type == content::ItemType::Equipment ||
               it->type == content::ItemType::Relic));
    }
}

TEST_CASE("boss drops: rollBossDrops assembles a reload-proof, gated outcome",
          "[bossdrops]") {
    content::ContentDatabase db;
    content::LoadReport rep;
    REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep));
    const std::vector<std::string> pool = legendaryDropPool(db);
    REQUIRE_FALSE(pool.empty());

    // Ineligible runs never drop.
    for (std::uint64_t s = 1; s <= 200; ++s) {
        const BossDropResult r = rollBossDrops(s * 6151u, 2, 20, db);
        CHECK(r.tokens == 0);
        CHECK_FALSE(r.legendary);
        CHECK(r.legendaryId.empty());
    }

    // Eligible runs: same seed -> same outcome; a token drop pays the town count;
    // a legendary drop names a pool member.
    for (std::uint64_t s = 1; s <= 2000; ++s) {
        const std::uint64_t seed = s * 2654435761u;
        const BossDropResult a = rollBossDrops(seed, 7, 20, db);
        const BossDropResult b = rollBossDrops(seed, 7, 20, db);
        CHECK(a.tokens == b.tokens);
        CHECK(a.legendary == b.legendary);
        CHECK(a.legendaryId == b.legendaryId);
        if (a.tokens > 0) {
            CHECK(a.tokens == 2);  // town 7 doubles
        }
        if (a.legendary) {
            CHECK(std::find(pool.begin(), pool.end(), a.legendaryId) != pool.end());
        } else {
            CHECK(a.legendaryId.empty());
        }
    }
}
