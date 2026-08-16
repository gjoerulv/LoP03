#include "editor/EditorValidation.hpp"

#include <algorithm>

#include "battle/Battle.hpp"
#include "battle/Simulator.hpp"
#include "content/ContentLoader.hpp"
#include "dungeon/DungeonModel.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"

namespace cd::editor {

namespace {

// Fixed seeds so a check's verdict never flickers between identical saves.
constexpr std::uint64_t kQuickCheckSeed = 0xC0FFEE59ull;

Party makeParty(const content::ContentDatabase& db, int level, GearTier tier) {
    Party party;
    for (const content::ClassDef* cls : defaultSimClasses(db)) {
        Character c = createCharacter(*cls, cls->name, level);
        c.weapon = pickSimGear(db, *cls, content::EquipSlot::Weapon, tier);
        c.armor = pickSimGear(db, *cls, content::EquipSlot::Armor, tier);
        c.accessory = pickSimGear(db, *cls, content::EquipSlot::Accessory, tier);
        refreshCharacter(c, db);
        party.members.push_back(std::move(c));
    }
    healFull(party);
    return party;
}

// The theme with the lexicographically first id (stable across runs).
const content::DungeonThemeDef* firstTheme(const content::ContentDatabase& db) {
    const content::DungeonThemeDef* best = nullptr;
    for (const auto& [id, theme] : db.themes()) {
        if (best == nullptr || id < best->id) {
            best = &theme;
        }
    }
    return best;
}

QuickCheckResult runBattle(const content::ContentDatabase& db, const char* name, Party party,
                           dungeon::EnemyTeam team) {
    QuickCheckResult result;
    result.name = name;
    if (party.empty() || team.count() == 0) {
        result.detail = "party or team could not be built";
        return result;
    }
    battle::Battle battle = battle::buildBattle(party, team, db);
    battle.rngSeed = kQuickCheckSeed;
    const battle::SimResult sim = battle::simulate(battle, db);
    result.ran = true;
    result.passed = sim.outcome == battle::Outcome::Victory;
    if (result.passed) {
        result.detail = "won in " + std::to_string(sim.rounds) + " turns, party HP " +
                        std::to_string(static_cast<int>(sim.partyHpFraction() * 100.0f)) + "%";
    } else {
        result.detail = sim.outcome == battle::Outcome::Defeat
                            ? "LOST - the curve may be broken"
                            : "did not finish in 300 rounds";
    }
    return result;
}

}  // namespace

std::vector<const content::ClassDef*> defaultSimClasses(const content::ContentDatabase& db) {
    std::vector<const content::ClassDef*> picked;
    for (const char* id : {"knight", "ranger", "mage", "cleric"}) {
        if (const content::ClassDef* cls = db.findClass(id)) {
            picked.push_back(cls);
        }
    }
    if (picked.size() == 4) {
        return picked;
    }
    // A renamed roster still produces a party: first four non-King classes.
    picked.clear();
    std::vector<std::string> ids;
    for (const auto& [id, cls] : db.classes()) {
        if (!cls.unlockedByKing) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    for (const std::string& id : ids) {
        if (picked.size() == 4) {
            break;
        }
        if (const content::ClassDef* cls = db.findClass(id)) {
            picked.push_back(cls);
        }
    }
    return picked;
}

std::string pickSimGear(const content::ContentDatabase& db, const content::ClassDef& cls,
                        content::EquipSlot slot, GearTier tier) {
    if (tier == GearTier::None || !cls.canEquip(slot)) {
        return {};
    }
    const bool magical = cls.baseStats.magic > cls.baseStats.attack;
    std::vector<const content::ItemDef*> options;
    for (const auto& [id, item] : db.items()) {
        const bool equippable =
            item.type == content::ItemType::Equipment || item.type == content::ItemType::Relic;
        if (equippable && item.slot == slot) {
            options.push_back(&item);
        }
    }
    if (options.empty()) {
        return {};
    }
    if (slot == content::EquipSlot::Weapon) {
        // A mage gets the best rod, not a spear.
        std::vector<const content::ItemDef*> preferred;
        for (const content::ItemDef* item : options) {
            const bool fits = magical ? item->statBonus.magic >= item->statBonus.attack
                                      : item->statBonus.attack >= item->statBonus.magic;
            if (fits) {
                preferred.push_back(item);
            }
        }
        if (!preferred.empty()) {
            options = std::move(preferred);
        }
    }
    std::sort(options.begin(), options.end(),
              [](const content::ItemDef* a, const content::ItemDef* b) {
                  return a->value != b->value ? a->value < b->value : a->id < b->id;
              });
    const std::size_t index =
        tier == GearTier::Best ? options.size() - 1 : options.size() / 2;
    return options[index]->id;
}

bool buildDatabase(const EditorDocs& docs, content::ContentDatabase& db,
                   content::LoadReport& rep) {
    struct Entry {
        Category category;
        void (*parse)(const content::Json&, const std::string&, content::ContentDatabase&,
                      content::LoadReport&);
    };
    // Skills first so cross-referencing content parses after its targets, the
    // same order loadAll uses.
    static const Entry kEntries[] = {
        {Category::Skills, &content::parseSkills},
        {Category::Classes, &content::parseClasses},
        {Category::Enemies, &content::parseEnemies},
        {Category::Items, &content::parseItems},
        {Category::Bosses, &content::parseBosses},
        {Category::Themes, &content::parseThemes},
        {Category::Passives, &content::parsePassives},
        {Category::Milestones, &content::parseMilestones},  // M63
        {Category::Composition, &content::parseComposition},
        {Category::Story, &content::parseStory},
        {Category::EventFlavor, &content::parseEventFlavor},  // M86
        {Category::CurioLore, &content::parseCurioLore},      // M86
        {Category::Cutscenes, &content::parseCutscenes},      // M97
    };
    for (const Entry& entry : kEntries) {
        const DocFile& doc = docs.file(entry.category);
        const std::string filename = infoFor(entry.category).filename;
        if (!doc.loaded) {
            rep.add(filename, "file", doc.loadError.empty() ? "not loaded" : doc.loadError);
            continue;
        }
        // ordered_json -> the loader's json (key order is irrelevant to it).
        const content::Json plain = content::Json::parse(doc.root.dump(), nullptr, false);
        if (plain.is_discarded()) {
            rep.add(filename, "file", "document did not re-serialize as JSON");
            continue;
        }
        entry.parse(plain, filename, db, rep);
    }
    content::validateReferences(db, rep);
    return rep.ok();
}

std::vector<QuickCheckResult> runQuickChecks(const content::ContentDatabase& db) {
    std::vector<QuickCheckResult> results;

    // 1. A fresh, naked level-1 party beats the first theme's opening pair.
    {
        QuickCheckResult r;
        r.name = "Fresh party wins the first fight";
        const content::DungeonThemeDef* theme = firstTheme(db);
        if (theme == nullptr || theme->normalEnemies.empty()) {
            r.detail = "no theme with normal enemies";
            results.push_back(r);
        } else {
            dungeon::EnemyTeam team;
            team.name = "Quick check";
            team.enemyIds.push_back(theme->normalEnemies.front());
            team.enemyIds.push_back(
                theme->normalEnemies[theme->normalEnemies.size() > 1 ? 1 : 0]);
            results.push_back(
                runBattle(db, r.name.c_str(), makeParty(db, 1, GearTier::None), team));
        }
    }

    // 2. A mid-ladder party (level 20, median gear) beats the first theme's
    //    first boss with its court at a town-3-ish 150% scale.
    {
        QuickCheckResult r;
        r.name = "Mid-ladder boss is beatable";
        const content::DungeonThemeDef* theme = firstTheme(db);
        const content::BossDef* boss = nullptr;
        if (theme != nullptr && !theme->bosses.empty()) {
            boss = db.findBoss(theme->bosses.front());
        }
        if (boss == nullptr) {
            r.detail = "no theme boss found";
            results.push_back(r);
        } else {
            dungeon::EnemyTeam team;
            team.name = boss->name;
            team.isBoss = true;
            team.bossId = boss->id;
            team.enemyIds = boss->minions;
            team.statScalePct = 150;
            results.push_back(
                runBattle(db, r.name.c_str(), makeParty(db, 20, GearTier::Median), team));
        }
    }

    // 3. A maxed legendary party clears the Boss Rush opener at its 580%
    //    castle scale. (Deliberately NOT the King: by the owner-approved M54
    //    balance, a party without the Royal-Relic counterplay loses to him,
    //    and the simulator's AI uses no items — so "sim beats the King"
    //    would be a broken check, not a broken curve.)
    {
        QuickCheckResult r;
        r.name = "Endgame gear clears the Boss Rush opener";
        const dungeon::EnemyTeam team = bossRushTeam(db, 0);
        if (team.bossId.empty()) {
            r.detail = "no boss available for the rush";
            results.push_back(r);
        } else {
            results.push_back(
                runBattle(db, r.name.c_str(), makeParty(db, kMaxLevel, GearTier::Best), team));
        }
    }

    return results;
}

ValidationResult validateDocs(const EditorDocs& docs) {
    ValidationResult result;
    content::ContentDatabase db;
    result.databaseOk = buildDatabase(docs, db, result.report);
    if (result.databaseOk) {
        result.checks = runQuickChecks(db);
    }
    return result;
}

}  // namespace cd::editor
