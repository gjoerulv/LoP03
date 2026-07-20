#include "content/ContentLoader.hpp"

#include <fstream>

#include "content/Enums.hpp"
#include "content/JsonValidation.hpp"

namespace cd::content {

namespace fs = std::filesystem;

namespace {

// Validates the { version, <arrayKey>: [...] } wrapper and invokes `fn(element,
// context, index)` for each object element. Returns early (without calling fn)
// if the wrapper or version is invalid.
template <typename Fn>
void forEachEntry(const Json& root, const std::string& source, const char* arrayKey,
                  LoadReport& rep, Fn&& fn) {
    if (!root.is_object()) {
        rep.add(source, "<root>", "expected a top-level JSON object");
        return;
    }

    auto versionIt = root.find("version");
    if (versionIt == root.end()) {
        rep.add(source, "version", "missing required integer");
        return;
    }
    if (!versionIt->is_number_integer()) {
        rep.add(source, "version", "expected integer");
        return;
    }
    if (versionIt->get<int>() != kContentSchemaVersion) {
        rep.add(source, "version",
                "unsupported schema version " + std::to_string(versionIt->get<int>()) +
                    " (expected " + std::to_string(kContentSchemaVersion) + ")");
        return;
    }

    auto arrayIt = root.find(arrayKey);
    if (arrayIt == root.end()) {
        rep.add(source, arrayKey, "missing required array");
        return;
    }
    if (!arrayIt->is_array()) {
        rep.add(source, arrayKey, "expected array");
        return;
    }

    int index = 0;
    for (const auto& element : *arrayIt) {
        const std::string ctx = std::string(arrayKey) + "[" + std::to_string(index) + "]";
        if (!element.is_object()) {
            rep.add(source, ctx, "expected object");
        } else {
            fn(element, ctx, index);
        }
        ++index;
    }
}

}  // namespace

void parseSkills(const Json& root, const std::string& source, ContentDatabase& db,
                 LoadReport& rep) {
    forEachEntry(root, source, "skills", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        SkillDef d;
        d.id = r.reqString("id");
        d.name = r.reqString("name");
        d.category = r.reqEnum<SkillCategory>("category", parseSkillCategory, "skill category");
        d.target = r.reqEnum<SkillTarget>("target", parseSkillTarget, "skill target");
        d.element = r.optEnum<Element>("element", parseElement, Element::None, "element");
        d.power = r.optIntMin("power", 0, 0);
        d.mpCost = r.optIntMin("mpCost", 0, 0);
        d.statusEffect =
            r.optEnum<StatusType>("statusEffect", parseStatusType, StatusType::None, "status type");
        d.statusMagnitude = r.optIntMin("statusMagnitude", 0, 0);
        d.statusDuration = r.optIntMin("statusDuration", 0, 0);
        d.controlEffect =
            r.optEnum<SkillEffect>("control", parseSkillEffect, SkillEffect::None, "control effect");
        d.description = r.optString("description");
        if (rep.errorCount() != before) {
            return;  // invalid entry; skip
        }
        if (!db.addSkill(d)) {
            rep.add(source, ctx, "duplicate skill id '" + d.id + "'");
        }
    });
}

void parseClasses(const Json& root, const std::string& source, ContentDatabase& db,
                  LoadReport& rep) {
    forEachEntry(root, source, "classes", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        ClassDef d;
        d.id = r.reqString("id");
        d.name = r.reqString("name");
        d.role = r.optString("role");
        d.baseStats = r.reqStatBlock("baseStats");
        d.growth = r.optStatGrowth("growth");
        d.startingSkills = r.optStringArray("startingSkills");
        // Optional level-gated learnset (M29): array of { skill, level >= 1 }.
        if (const auto it = el.find("learnset"); it != el.end()) {
            if (!it->is_array()) {
                rep.add(source, ctx + ".learnset", "expected array");
            } else {
                int li = 0;
                for (const auto& le : *it) {
                    const std::string lctx = ctx + ".learnset[" + std::to_string(li) + "]";
                    if (!le.is_object()) {
                        rep.add(source, lctx, "expected object");
                    } else {
                        ObjectReader lr(le, lctx, source, rep);
                        LearnEntry entry;
                        entry.skill = lr.reqString("skill");
                        entry.level = lr.reqIntMin("level", 1);
                        d.learnset.push_back(std::move(entry));
                    }
                    ++li;
                }
            }
        }
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addClass(d)) {
            rep.add(source, ctx, "duplicate class id '" + d.id + "'");
        }
    });
}

void parseEnemies(const Json& root, const std::string& source, ContentDatabase& db,
                  LoadReport& rep) {
    forEachEntry(root, source, "enemies", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        EnemyDef d;
        d.id = r.reqString("id");
        d.name = r.reqString("name");
        d.stats = r.reqStatBlock("stats");
        d.tier = r.optEnum<EnemyTier>("tier", parseEnemyTier, EnemyTier::Normal, "enemy tier");
        d.role = r.reqEnum<EnemyRole>("role", parseEnemyRole, "enemy role");
        for (const auto& tag : r.optStringArray("tags")) {
            if (auto parsed = parseEnemyTag(tag)) {
                d.tags.push_back(*parsed);
            } else {
                rep.add(source, ctx + ".tags", "unknown enemy tag '" + tag + "'");
            }
        }
        d.skills = r.optStringArray("skills");
        d.xpReward = r.optIntMin("xpReward", 0, 0);
        d.goldReward = r.optIntMin("goldReward", 0, 0);
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addEnemy(d)) {
            rep.add(source, ctx, "duplicate enemy id '" + d.id + "'");
        }
    });
}

void parseItems(const Json& root, const std::string& source, ContentDatabase& db,
                LoadReport& rep) {
    forEachEntry(root, source, "items", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        ItemDef d;
        d.id = r.reqString("id");
        d.name = r.reqString("name");
        d.type = r.reqEnum<ItemType>("type", parseItemType, "item type");
        d.slot = r.optEnum<EquipSlot>("slot", parseEquipSlot, EquipSlot::None, "equip slot");
        d.rarity = r.optEnum<Rarity>("rarity", parseRarity, Rarity::Common, "rarity");
        d.value = r.optIntMin("value", 0, 0);
        d.effect = r.optEnum<ConsumableEffect>("effect", parseConsumableEffect,
                                               ConsumableEffect::None, "consumable effect");
        d.effectAmount = r.optIntMin("effectAmount", 0, 0);
        d.statBonus = r.optStatBlock("statBonus");
        d.grantsSkill = r.optString("grantsSkill");
        d.description = r.optString("description");

        // Semantic rules tying fields together.
        if (d.type == ItemType::Scroll && d.grantsSkill.empty()) {
            rep.add(source, ctx, "scroll item must specify a non-empty 'grantsSkill'");
        }
        if ((d.type == ItemType::Equipment || d.type == ItemType::Relic) &&
            d.slot == EquipSlot::None) {
            rep.add(source, ctx, "equipment/relic must specify a non-'none' 'slot'");
        }

        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addItem(d)) {
            rep.add(source, ctx, "duplicate item id '" + d.id + "'");
        }
    });
}

void parseBosses(const Json& root, const std::string& source, ContentDatabase& db,
                 LoadReport& rep) {
    forEachEntry(root, source, "bosses", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        BossDef d;
        d.id = r.reqString("id");
        d.name = r.reqString("name");
        d.archetype =
            r.reqEnum<BossArchetype>("archetype", parseBossArchetype, "boss archetype");
        d.stats = r.reqStatBlock("stats");
        d.skills = r.optStringArray("skills");
        d.minions = r.optStringArray("minions");
        d.telegraph = r.optString("telegraph");
        d.xpReward = r.optIntMin("xpReward", 0, 0);
        d.goldReward = r.optIntMin("goldReward", 0, 0);
        d.description = r.optString("description");
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addBoss(d)) {
            rep.add(source, ctx, "duplicate boss id '" + d.id + "'");
        }
    });
}

void parseThemes(const Json& root, const std::string& source, ContentDatabase& db,
                 LoadReport& rep) {
    forEachEntry(root, source, "themes", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        DungeonThemeDef d;
        d.id = r.reqString("id");
        d.name = r.reqString("name");
        d.normalEnemies = r.optStringArray("normalEnemies");
        d.eliteEnemies = r.optStringArray("eliteEnemies");
        d.bosses = r.optStringArray("bosses");
        d.description = r.optString("description");
        if (d.normalEnemies.empty()) {
            rep.add(source, ctx, "theme must list at least one entry in 'normalEnemies'");
        }
        if (d.bosses.empty()) {
            rep.add(source, ctx, "theme must list at least one entry in 'bosses'");
        }
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addTheme(d)) {
            rep.add(source, ctx, "duplicate theme id '" + d.id + "'");
        }
    });
}

void validateReferences(const ContentDatabase& db, LoadReport& rep) {
    const std::string source = "<references>";
    for (const auto& [id, cls] : db.classes()) {
        for (const auto& skill : cls.startingSkills) {
            if (!db.hasSkill(skill)) {
                rep.add(source, "class '" + id + "'.startingSkills",
                        "references unknown skill '" + skill + "'");
            }
        }
        for (const auto& entry : cls.learnset) {
            if (!db.hasSkill(entry.skill)) {
                rep.add(source, "class '" + id + "'.learnset",
                        "references unknown skill '" + entry.skill + "'");
            }
        }
    }
    for (const auto& [id, enemy] : db.enemies()) {
        for (const auto& skill : enemy.skills) {
            if (!db.hasSkill(skill)) {
                rep.add(source, "enemy '" + id + "'.skills",
                        "references unknown skill '" + skill + "'");
            }
        }
    }
    for (const auto& [id, item] : db.items()) {
        if (item.type == ItemType::Scroll && !item.grantsSkill.empty() &&
            !db.hasSkill(item.grantsSkill)) {
            rep.add(source, "item '" + id + "'.grantsSkill",
                    "references unknown skill '" + item.grantsSkill + "'");
        }
    }
    for (const auto& [id, boss] : db.bosses()) {
        for (const auto& skill : boss.skills) {
            if (!db.hasSkill(skill)) {
                rep.add(source, "boss '" + id + "'.skills",
                        "references unknown skill '" + skill + "'");
            }
        }
        for (const auto& minion : boss.minions) {
            if (db.findEnemy(minion) == nullptr) {
                rep.add(source, "boss '" + id + "'.minions",
                        "references unknown enemy '" + minion + "'");
            }
        }
    }
    for (const auto& [id, theme] : db.themes()) {
        for (const auto& enemy : theme.normalEnemies) {
            if (db.findEnemy(enemy) == nullptr) {
                rep.add(source, "theme '" + id + "'.normalEnemies",
                        "references unknown enemy '" + enemy + "'");
            }
        }
        for (const auto& enemy : theme.eliteEnemies) {
            if (db.findEnemy(enemy) == nullptr) {
                rep.add(source, "theme '" + id + "'.eliteEnemies",
                        "references unknown enemy '" + enemy + "'");
            }
        }
        for (const auto& boss : theme.bosses) {
            if (db.findBoss(boss) == nullptr) {
                rep.add(source, "theme '" + id + "'.bosses",
                        "references unknown boss '" + boss + "'");
            }
        }
    }
}

bool readJsonFile(const fs::path& file, Json& out, LoadReport& rep) {
    const std::string source = file.filename().string();
    std::error_code ec;
    if (!fs::exists(file, ec) || ec) {
        rep.add(source, "<file>", "file not found: " + file.string());
        return false;
    }
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        rep.add(source, "<file>", "could not open file: " + file.string());
        return false;
    }
    // allow_exceptions = false -> returns a discarded value instead of throwing.
    out = Json::parse(stream, nullptr, false);
    if (out.is_discarded()) {
        rep.add(source, "<file>", "invalid JSON (parse error)");
        return false;
    }
    return true;
}

void parseComposition(const Json& root, const std::string& source, ContentDatabase& db,
                      LoadReport& rep) {
    if (!root.is_object()) {
        rep.add(source, "<root>", "expected a top-level JSON object");
        return;
    }
    ObjectReader r(root, "composition", source, rep);
    const int version = r.reqInt("version");
    if (version != 1) {
        rep.add(source, "version", "unsupported composition version");
        return;
    }
    CompositionDef c;  // defaults are the owner-approved curves
    if (const auto it = root.find("team"); it != root.end() && it->is_object()) {
        ObjectReader t(*it, "composition.team", source, rep);
        c.minSize = t.optIntMin("minSize", 1, c.minSize);
        c.deepMinSize = t.optIntMin("deepMinSize", 1, c.deepMinSize);
        c.deepMinDepth = t.optIntMin("deepMinDepth", 1, c.deepMinDepth);
        c.maxSizeBase = t.optIntMin("maxSizeBase", 1, c.maxSizeBase);
        c.maxSizePerDepths = t.optIntMin("maxSizePerDepths", 1, c.maxSizePerDepths);
        c.maxSizeCap = t.optIntMin("maxSizeCap", 1, c.maxSizeCap);
        c.elitePctPerDepth = t.optIntMin("elitePctPerDepth", 0, c.elitePctPerDepth);
        c.elitePctMax = t.optIntMin("elitePctMax", 0, c.elitePctMax);
        c.maxSupport = t.optIntMin("maxSupport", 0, c.maxSupport);
        c.minDamage = t.optIntMin("minDamage", 0, c.minDamage);
    }
    if (const auto it = root.find("boss"); it != root.end() && it->is_object()) {
        ObjectReader b(*it, "composition.boss", source, rep);
        c.minMinions = b.optIntMin("minMinions", 0, c.minMinions);
        c.maxMinions = b.optIntMin("maxMinions", 0, c.maxMinions);
    }
    if (const auto it = root.find("statScale"); it != root.end() && it->is_object()) {
        ObjectReader s(*it, "composition.statScale", source, rep);
        c.scaleStartDepth = s.optIntMin("startDepth", 1, c.scaleStartDepth);
        c.scalePctPerDepth = s.optIntMin("pctPerDepth", 0, c.scalePctPerDepth);
        c.scalePctMax = s.optIntMin("pctMax", 0, c.scalePctMax);
    }
    // Relational sanity: report and repair rather than ship an impossible rule.
    if (c.maxSizeCap < c.deepMinSize || c.maxSizeCap < c.minSize) {
        rep.add(source, "team", "maxSizeCap below minimum sizes; raised to match");
        c.maxSizeCap = c.deepMinSize > c.minSize ? c.deepMinSize : c.minSize;
    }
    if (c.maxMinions < c.minMinions) {
        rep.add(source, "boss", "maxMinions below minMinions; raised to match");
        c.maxMinions = c.minMinions;
    }
    db.setComposition(c);
}

bool loadAll(const fs::path& dataRoot, ContentDatabase& db, LoadReport& rep) {
    const std::size_t before = rep.errorCount();

    Json json;
    // Skills first so class/enemy/scroll references can be validated.
    if (readJsonFile(dataRoot / "skills.json", json, rep)) {
        parseSkills(json, "skills.json", db, rep);
    }
    if (readJsonFile(dataRoot / "classes.json", json, rep)) {
        parseClasses(json, "classes.json", db, rep);
    }
    if (readJsonFile(dataRoot / "enemies.json", json, rep)) {
        parseEnemies(json, "enemies.json", db, rep);
    }
    if (readJsonFile(dataRoot / "items.json", json, rep)) {
        parseItems(json, "items.json", db, rep);
    }
    if (readJsonFile(dataRoot / "bosses.json", json, rep)) {
        parseBosses(json, "bosses.json", db, rep);
    }
    if (readJsonFile(dataRoot / "dungeon_themes.json", json, rep)) {
        parseThemes(json, "dungeon_themes.json", db, rep);
    }
    if (readJsonFile(dataRoot / "composition.json", json, rep)) {
        parseComposition(json, "composition.json", db, rep);
    }

    validateReferences(db, rep);
    return rep.errorCount() == before;
}

}  // namespace cd::content
