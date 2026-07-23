#include "save/SaveSystem.hpp"

#include "platform/AtomicFile.hpp"

#include <algorithm>
#include <utility>

#include <nlohmann/json.hpp>

#include "content/ContentDatabase.hpp"
#include "content/ContentLoader.hpp"
#include "content/JsonValidation.hpp"
#include "game/Party.hpp"
#include "game/WorldLadder.hpp"

namespace cd::save {
namespace fs = std::filesystem;
using content::Json;

const char* slotFileStem(SaveSlot slot) {
  switch (slot) {
  case SaveSlot::Auto: return "save_auto";
  case SaveSlot::Manual1: return "save_slot1";
  case SaveSlot::Manual2: return "save_slot2";
  case SaveSlot::Manual3: return "save_slot3";
  }
  return "save_slot1";
}

const char* slotDisplayName(SaveSlot slot) {
  switch (slot) {
  case SaveSlot::Auto: return "Autosave";
  case SaveSlot::Manual1: return "Slot 1";
  case SaveSlot::Manual2: return "Slot 2";
  case SaveSlot::Manual3: return "Slot 3";
  }
  return "Slot";
}

SaveSystem::SaveSystem(const content::ContentDatabase& db, fs::path directory)
    : db_(db), dir_(std::move(directory)) {}

fs::path SaveSystem::slotPath(SaveSlot slot) const {
  return dir_ / (std::string(slotFileStem(slot)) + ".json");
}

bool SaveSystem::exists(SaveSlot slot) const {
  std::error_code ec;
  return fs::exists(slotPath(slot), ec) && !ec;
}

bool SaveSystem::save(SaveSlot slot, const Party& party,
                      content::LoadReport& report) const {
  const std::string source = slotPath(slot).filename().string();
  Json root;
  root["version"] = kSaveVersion;
  root["gold"] = party.gold;
  root["restTokens"] = party.restTokens;  // M30 (optional; old saves read 0)
  root["currentTown"] = party.currentTown;              // M32 (optional; old -> 1)
  root["highestUnlockedTown"] = party.highestUnlockedTown;  // M32 (optional; old -> 1)
  root["stakesPrevTown"] = party.stakes.prevTown;          // M33 (optional; old -> 0)
  root["stakesPrevDepth"] = party.stakes.prevDepth;        // M33
  root["stakesPenaltySteps"] = party.stakes.penaltySteps;  // M33
  root["legendaryTokens"] = party.legendaryTokens;         // M34 (optional; old -> 0)
  root["blackMarketPresent"] = party.blackMarket.present ? 1 : 0;  // M34
  root["blackMarketTown"] = party.blackMarket.town;
  root["blackMarketItem"] = party.blackMarket.itemId;
  root["blackMarketPrice"] = party.blackMarket.priceGold;
  root["blackMarketTileX"] = party.blackMarket.tileX;
  root["blackMarketTileY"] = party.blackMarket.tileY;
  root["castleUnlocked"] = party.castleUnlocked;                        // M40 (optional; old -> false)
  root["castleBossRushBestTurns"] = party.castleRecords.bossRushBestTurns;
  root["castleEndlessBestWave"] = party.castleRecords.endlessBestWave;
  root["castleKingDefeated"] = party.castleRecords.kingDefeated;
  root["castleKingBestTurns"] = party.castleRecords.kingBestTurns;
  root["castleKingTitle"] = party.castleRecords.kingTitle;
  root["storyMet"] = party.storyMet;  // M41 (optional; old -> 0)
  root["encountered"] = party.encountered;             // M42 (optional; old -> empty)
  root["recordBiggestHit"] = party.recordBiggestHit;   // M42 (optional; old -> 0)
  root["recordRunDamage"] = party.recordRunDamage;     // M42 (optional; old -> 0)

  Json members = Json::array();
  for (const Character& c : party.members) {
    Json m;
    m["classId"] = c.classId;
    m["name"] = c.name;
    m["level"] = c.level;
    m["xp"] = c.xp;
    m["hp"] = c.hp;
    m["mp"] = c.mp;
    m["weapon"] = c.weapon;
    m["armor"] = c.armor;
    m["accessory"] = c.accessory;
    m["ownedPassives"] = c.ownedPassives;      // M36 (optional; old saves -> empty)
    m["equippedPassive"] = c.equippedPassive;  // M36
    members.push_back(std::move(m));
  }
  root["party"] = std::move(members);

  Json items = Json::array();
  for (const ItemStack& stack : party.inventory.stacks) {
    Json js;
    js["itemId"] = stack.itemId;
    js["count"] = stack.count;
    items.push_back(std::move(js));
  }
  root["inventory"] = std::move(items);

  const std::string serialized = root.dump(2) + '\n';
  std::string writeError;
  if (!platform::writeTextFileAtomically(slotPath(slot), serialized, writeError,
                                         platform::AtomicWriteOptions{true})) {
    report.add(source, "<file>", "could not save atomically: " + writeError);
    return false;
  }
  return true;
}

bool SaveSystem::autosave(const Party& party, content::LoadReport& report) const {
  return save(SaveSlot::Auto, party, report);
}

bool SaveSystem::load(SaveSlot slot, Party& outParty,
                      content::LoadReport& report) const {
  const std::size_t before = report.errorCount();
  const std::string source = slotPath(slot).filename().string();
  Json root;
  if (!content::readJsonFile(slotPath(slot), root, report)) {
    return false;
  }
  if (!root.is_object()) {
    report.add(source, "", "expected a top-level JSON object");
    return false;
  }

  content::ObjectReader rootReader(root, "save", source, report);
  const int version = rootReader.reqInt("version");
  if (version != kSaveVersion) {
    report.add(source, "version",
               "unsupported save version " + std::to_string(version) +
                   " (expected " + std::to_string(kSaveVersion) + ")");
    return false;
  }

  Party loaded;
  loaded.gold = rootReader.optIntMin("gold", 0, 0);
  loaded.restTokens = rootReader.optIntMin("restTokens", 0, 0);  // M30
  // M32 town ladder: optional, clamped to [1, kTownCount]; current town can
  // never exceed the highest unlocked town (defensive against a tampered file).
  loaded.highestUnlockedTown =
      clampTown(rootReader.optIntMin("highestUnlockedTown", 1, 1));
  loaded.currentTown = std::clamp(clampTown(rootReader.optIntMin("currentTown", 1, 1)),
                                  1, loaded.highestUnlockedTown);
  // M33 stakes: optional, non-negative; steps clamped to [0, kStakesPenaltyMaxSteps].
  loaded.stakes.prevTown = rootReader.optIntMin("stakesPrevTown", 0, 0);
  loaded.stakes.prevDepth = rootReader.optIntMin("stakesPrevDepth", 0, 0);
  loaded.stakes.penaltySteps = clampStakesSteps(rootReader.optIntMin("stakesPenaltySteps", 0, 0));
  // M34 black market: optional; an offer whose item the content no longer knows
  // is dropped (like unknown equipment ids), so a stale save never dangles.
  loaded.legendaryTokens = rootReader.optIntMin("legendaryTokens", 0, 0);
  loaded.blackMarket.present = rootReader.optIntMin("blackMarketPresent", 0, 0) != 0;
  loaded.blackMarket.town = rootReader.optIntMin("blackMarketTown", 0, 0);
  loaded.blackMarket.itemId = rootReader.optString("blackMarketItem");
  loaded.blackMarket.priceGold = rootReader.optIntMin("blackMarketPrice", 0, 0);
  loaded.blackMarket.tileX = rootReader.optIntMin("blackMarketTileX", 0, 0);
  loaded.blackMarket.tileY = rootReader.optIntMin("blackMarketTileY", 0, 0);
  if (loaded.blackMarket.present && db_.findItem(loaded.blackMarket.itemId) == nullptr) {
    loaded.blackMarket = BlackMarketOffer{};
  }
  // M50: the town shrank (26x15 -> 24x12), so a market offer saved before M50
  // may carry a tile that is now a wall or out of bounds — the NPC would be
  // unreachable. Snap any offer not on a current plaza tile onto a valid one, so
  // a stale offer stays claimable. Defensive-load only; no schema change.
  if (loaded.blackMarket.present) {
    bool onValidTile = false;
    for (const MarketTile& t : kBlackMarketTiles) {
      if (t.x == loaded.blackMarket.tileX && t.y == loaded.blackMarket.tileY) {
        onValidTile = true;
        break;
      }
    }
    if (!onValidTile) {
      loaded.blackMarket.tileX = kBlackMarketTiles[0].x;
      loaded.blackMarket.tileY = kBlackMarketTiles[0].y;
    }
  }
  // M40 castle: optional; old saves load locked with no records. Records are
  // non-negative; the King title is free text (defensive-loaded like any string).
  loaded.castleUnlocked = rootReader.optBool("castleUnlocked", false);
  loaded.castleRecords.bossRushBestTurns = rootReader.optIntMin("castleBossRushBestTurns", 0, 0);
  loaded.castleRecords.endlessBestWave = rootReader.optIntMin("castleEndlessBestWave", 0, 0);
  loaded.castleRecords.kingDefeated = rootReader.optBool("castleKingDefeated", false);
  loaded.castleRecords.kingBestTurns = rootReader.optIntMin("castleKingBestTurns", 0, 0);
  loaded.castleRecords.kingTitle = rootReader.optString("castleKingTitle");
  loaded.storyMet = rootReader.optIntMin("storyMet", 0, 0);  // M41 (optional; old -> 0)
  loaded.encountered = rootReader.optStringArray("encountered");  // M42 (optional; old -> empty)
  loaded.recordBiggestHit = rootReader.optIntMin("recordBiggestHit", 0, 0);  // M42
  loaded.recordRunDamage = rootReader.optIntMin("recordRunDamage", 0, 0);    // M42

  auto partyIt = root.find("party");
  if (partyIt == root.end() || !partyIt->is_array()) {
    report.add(source, "party", "expected array");
    return false;
  }

  int index = 0;
  for (const auto& element : *partyIt) {
    const std::string ctx = "party[" + std::to_string(index) + "]";
    ++index;
    if (!element.is_object()) {
      report.add(source, ctx, "expected object");
      continue;
    }
    const std::size_t elementBefore = report.errorCount();
    content::ObjectReader m(element, ctx, source, report);
    const std::string classId = m.reqString("classId");
    const std::string name = m.reqString("name");
    const int level = m.optIntMin("level", 1, 1);
    const int xp = m.optIntMin("xp", 0, 0);
    const int hp = m.optIntMin("hp", 0, 0);
    const int mp = m.optIntMin("mp", 0, 0);
    const std::string weapon = m.optString("weapon");
    const std::string armor = m.optString("armor");
    const std::string accessory = m.optString("accessory");
    const std::vector<std::string> ownedPassives = m.optStringArray("ownedPassives");  // M36
    const std::string equippedPassive = m.optString("equippedPassive");                // M36
    if (report.errorCount() != elementBefore) {
      continue;
    }
    const content::ClassDef* cls = db_.findClass(classId);
    if (cls == nullptr) {
      report.add(source, ctx + ".classId", "unknown class '" + classId + "'");
      continue;
    }
    Character c = createCharacter(*cls, name, level);
    c.xp = xp;
    c.weapon = (weapon.empty() || db_.findItem(weapon) != nullptr) ? weapon : "";
    c.armor = (armor.empty() || db_.findItem(armor) != nullptr) ? armor : "";
    c.accessory =
        (accessory.empty() || db_.findItem(accessory) != nullptr) ? accessory : "";
    // M36 passives: keep only owned ids the content still knows; an equipped id
    // is honored only if it is known and owned (dropped otherwise).
    for (const std::string& pid : ownedPassives) {
      if (db_.findPassive(pid) != nullptr &&
          std::find(c.ownedPassives.begin(), c.ownedPassives.end(), pid) ==
              c.ownedPassives.end()) {
        c.ownedPassives.push_back(pid);
      }
    }
    if (!equippedPassive.empty() && db_.findPassive(equippedPassive) != nullptr &&
        std::find(c.ownedPassives.begin(), c.ownedPassives.end(), equippedPassive) !=
            c.ownedPassives.end()) {
      c.equippedPassive = equippedPassive;
    }
    refreshCharacter(c, db_);
    c.hp = std::clamp(hp, 0, c.maxHp);
    c.mp = std::clamp(mp, 0, c.maxMp);
    loaded.members.push_back(std::move(c));
  }

  if (loaded.members.size() > kMaxPartySize) {
    report.add(source, "party",
               "too many members (" + std::to_string(loaded.members.size()) +
                   ", max " + std::to_string(kMaxPartySize) + ")");
  }

  if (auto invIt = root.find("inventory"); invIt != root.end()) {
    if (!invIt->is_array()) {
      report.add(source, "inventory", "expected array");
    } else {
      int invIndex = 0;
      for (const auto& element : *invIt) {
        const std::string ctx = "inventory[" + std::to_string(invIndex) + "]";
        ++invIndex;
        if (!element.is_object()) {
          report.add(source, ctx, "expected object");
          continue;
        }
        const std::size_t elementBefore = report.errorCount();
        content::ObjectReader ir(element, ctx, source, report);
        const std::string itemId = ir.reqString("itemId");
        const int count = ir.optIntMin("count", 1, 1);
        if (report.errorCount() != elementBefore) {
          continue;
        }
        if (db_.findItem(itemId) == nullptr) {
          report.add(source, ctx + ".itemId", "unknown item '" + itemId + "'");
          continue;
        }
        loaded.inventory.add(itemId, count);
      }
    }
  }

  if (report.errorCount() != before) {
    return false;
  }
  outParty = std::move(loaded);
  return true;
}

std::optional<SlotSummary> SaveSystem::summary(SaveSlot slot) const {
  Party tmp;
  content::LoadReport rep;
  if (!load(slot, tmp, rep)) {
    return std::nullopt;
  }
  SlotSummary s;
  s.partySize = static_cast<int>(tmp.size());
  s.highestLevel = highestLevel(tmp);
  s.gold = tmp.gold;
  s.kingTitle = tmp.castleRecords.kingTitle;  // M40
  return s;
}

} // namespace cd::save
