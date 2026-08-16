// M78 — inventory caps & shop UX: the pure cap helper (capFor / canBuyMore /
// merchantPriceFor), the loader rules for `maxHeld` / `notSoldInTown`, the
// premium tonics' town delisting, and the shipped data carrying it all.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/LoadReport.hpp"
#include "game/Inventory.hpp"
#include "game/ItemCaps.hpp"
#include "states/ItemShopFilter.hpp"

using namespace cd;

namespace {

content::Json parse(const char* text) { return content::Json::parse(text, nullptr, false); }

content::ItemDef consumable(const char* id, int maxHeld = 0) {
    content::ItemDef d;
    d.id = id;
    d.name = id;
    d.type = content::ItemType::Consumable;
    d.value = 50;
    d.maxHeld = maxHeld;
    return d;
}

const content::ContentDatabase& shipped() {
    static content::ContentDatabase db;
    static bool loaded = false;
    if (!loaded) {
        content::LoadReport rep;
        REQUIRE(content::loadAll(std::filesystem::path(CRYSTAL_TEST_DATA_DIR), db, rep));
        loaded = true;
    }
    return db;
}

}  // namespace

// --- the cap math -------------------------------------------------------------

TEST_CASE("caps: consumables cap at 2, authored exceptions win, gear is uncapped",
          "[caps]") {
    CHECK(capFor(consumable("antidote")) == 2);
    CHECK(capFor(consumable("potion", 9)) == 9);
    CHECK(capFor(consumable("hi_potion", 6)) == 6);

    content::ItemDef gear;
    gear.id = "blade";
    gear.type = content::ItemType::Equipment;
    CHECK(capFor(gear) == 0);  // uncapped
    gear.type = content::ItemType::Relic;
    CHECK(capFor(gear) == 0);
    gear.type = content::ItemType::Scroll;
    CHECK(capFor(gear) == 0);
}

TEST_CASE("caps: the perk bonus raises every cap by one, against a hard ceiling of 9",
          "[caps]") {
    CHECK(capFor(consumable("antidote"), 1) == 3);
    CHECK(capFor(consumable("hi_potion", 6), 1) == 7);
    CHECK(capFor(consumable("potion", 9), 1) == 9);   // Potion stays 9
    CHECK(capFor(consumable("antidote"), 50) == 9);   // the ceiling holds
    CHECK(capFor(consumable("antidote"), -3) == 2);   // a bad bonus never lowers
}

TEST_CASE("caps: >= at purchase - at-cap refuses, an overage is tolerated, never clamped",
          "[caps]") {
    const content::ItemDef pot = consumable("potion", 9);
    const content::ItemDef anti = consumable("antidote");
    Inventory inv;
    CHECK(canBuyMore(inv, anti));
    inv.add("antidote", 1);
    CHECK(canBuyMore(inv, anti));
    inv.add("antidote", 1);
    CHECK_FALSE(canBuyMore(inv, anti));  // at the cap

    // An older save may hold far more than a cap: the shop refuses another
    // sale but the stack itself is untouched.
    inv.add("potion", 12);
    CHECK_FALSE(canBuyMore(inv, pot));
    CHECK(inv.count("potion") == 12);

    content::ItemDef gear;
    gear.id = "blade";
    gear.type = content::ItemType::Equipment;
    inv.add("blade", 30);
    CHECK(canBuyMore(inv, gear));  // gear never caps
}

TEST_CASE("caps: the merchant asks full value for a premium tonic, street price otherwise",
          "[caps]") {
    content::ItemDef tonic = consumable("elixir");
    tonic.value = 400;
    tonic.notSoldInTown = true;
    CHECK(merchantPriceFor(tonic, 300) == 400);  // full value, not the 75% street price

    content::ItemDef plain = consumable("potion");
    plain.value = 25;
    CHECK(merchantPriceFor(plain, 18) == 18);  // the generated discount stands
}

// --- the loader rules ---------------------------------------------------------

TEST_CASE("caps: the loader rejects misplaced or over-ceiling cap fields", "[caps]") {
    SECTION("maxHeld on equipment") {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseItems(parse(R"({"version":1,"items":[
            {"id":"blade","name":"Blade","type":"equipment","slot":"weapon",
             "value":100,"iconCategory":"sword","maxHeld":2}]})"),
                            "items", db, rep);
        CHECK_FALSE(rep.ok());
    }
    SECTION("maxHeld above the ceiling") {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseItems(parse(R"({"version":1,"items":[
            {"id":"p","name":"P","type":"consumable","value":10,"maxHeld":10}]})"),
                            "items", db, rep);
        CHECK_FALSE(rep.ok());
    }
    SECTION("notSoldInTown on equipment") {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseItems(parse(R"({"version":1,"items":[
            {"id":"blade","name":"Blade","type":"equipment","slot":"weapon",
             "value":100,"iconCategory":"sword","notSoldInTown":true}]})"),
                            "items", db, rep);
        CHECK_FALSE(rep.ok());
    }
    SECTION("the valid shapes pass") {
        content::ContentDatabase db;
        content::LoadReport rep;
        content::parseItems(parse(R"({"version":1,"items":[
            {"id":"p","name":"P","type":"consumable","value":10,"maxHeld":9},
            {"id":"e","name":"E","type":"consumable","value":10,"notSoldInTown":true}]})"),
                            "items", db, rep);
        CHECK(rep.ok());
    }
}

// --- the shipped data ---------------------------------------------------------

TEST_CASE("caps: the shipped exceptions are Potion 9 and Hi-Potion 6", "[caps]") {
    CHECK(capFor(*shipped().findItem("potion")) == 9);
    CHECK(capFor(*shipped().findItem("hi_potion")) == 6);
    // Everyone else rides the type default.
    CHECK(capFor(*shipped().findItem("antidote")) == 2);
    CHECK(capFor(*shipped().findItem("ether")) == 2);
    CHECK(capFor(*shipped().findItem("elixir")) == 2);
    CHECK(capFor(*shipped().findItem("phoenix_tear")) == 2);
    CHECK(capFor(*shipped().findItem("holy_taxes")) == 2);
}

TEST_CASE("caps: the premium tonics left the town shelves, and only they did", "[caps]") {
    const content::ItemDef* elixir = shipped().findItem("elixir");
    const content::ItemDef* hiEther = shipped().findItem("hi_ether");
    REQUIRE(elixir != nullptr);
    REQUIRE(hiEther != nullptr);
    CHECK(elixir->notSoldInTown);
    CHECK(hiEther->notSoldInTown);
    int flagged = 0;
    for (const auto& [id, def] : shipped().items()) {
        if (def.notSoldInTown) {
            ++flagged;
        }
    }
    CHECK(flagged == 2);

    const auto contains = [](const std::vector<std::string>& v, const char* id) {
        for (const std::string& s : v) {
            if (s == id) {
                return true;
            }
        }
        return false;
    };
    for (int town = 1; town <= 7; ++town) {
        const std::vector<std::string> ids = itemShopBuyIds(shipped(), town);
        INFO("town " << town);
        CHECK_FALSE(contains(ids, "elixir"));
        CHECK_FALSE(contains(ids, "hi_ether"));
        CHECK(contains(ids, "potion"));  // ordinary stock is untouched
        CHECK(contains(ids, "ether"));   // the small tonic still sells
    }
}

// --- the M88 shelf order ------------------------------------------------------

TEST_CASE("shop: the shelf reads by purpose - heals, MP, cures, revives, oddities (M88)",
          "[caps][shop]") {
    // Category ranks are the owner's order, Potion first.
    CHECK(itemShopCategoryRank(*shipped().findItem("potion")) == 0);
    CHECK(itemShopCategoryRank(*shipped().findItem("ether")) == 1);
    CHECK(itemShopCategoryRank(*shipped().findItem("antidote")) == 2);
    CHECK(itemShopCategoryRank(*shipped().findItem("phoenix_tear")) == 3);
    CHECK(itemShopCategoryRank(*shipped().findItem("royal_snacks")) == 4);

    // The whole shelf is monotone in (rank, value, id) at every town, and the
    // shipped town-1 shelf comes out in exactly the intended reading order.
    for (int town = 1; town <= 7; ++town) {
        const std::vector<std::string> ids = itemShopBuyIds(shipped(), town);
        INFO("town " << town);
        for (std::size_t i = 1; i < ids.size(); ++i) {
            const content::ItemDef* a = shipped().findItem(ids[i - 1]);
            const content::ItemDef* b = shipped().findItem(ids[i]);
            REQUIRE(a != nullptr);
            REQUIRE(b != nullptr);
            const int ra = itemShopCategoryRank(*a);
            const int rb = itemShopCategoryRank(*b);
            INFO(ids[i - 1] << " before " << ids[i]);
            CHECK(ra <= rb);
            if (ra == rb) {
                CHECK((a->value < b->value || (a->value == b->value && a->id < b->id)));
            }
        }
    }
    const std::vector<std::string> town1 = itemShopBuyIds(shipped(), 1);
    const std::vector<std::string> expected = {"potion",       "hi_potion", "mega_potion",
                                               "ether",        "antidote",  "phoenix_tear",
                                               "royal_snacks"};
    CHECK(town1 == expected);
    REQUIRE_FALSE(town1.empty());
    CHECK(town1.front() == "potion");  // the owner's headline requirement
}
