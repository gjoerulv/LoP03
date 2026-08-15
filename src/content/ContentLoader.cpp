#include "content/ContentLoader.hpp"

#include <array>
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

// M48: reads the optional `weaknesses[]` / `immunities[]` element arrays shared
// by enemies and bosses. `none` is rejected rather than ignored — it means
// nothing and is far more likely to be a typo than an intent. The two sets must
// be disjoint: a foe that is both weak and immune to an element is a content
// error, not a precedence puzzle for the damage code to answer.
ElementAffinity readAffinity(ObjectReader& r, const std::string& source, const std::string& ctx,
                             LoadReport& rep) {
    ElementAffinity a;
    const auto readList = [&](const char* key, std::vector<Element>& out) {
        for (const std::string& name : r.optStringArray(key)) {
            const std::optional<Element> parsed = parseElement(name);
            if (!parsed) {
                rep.add(source, ctx, "unknown element '" + name + "' in '" + key + "'");
            } else if (*parsed == Element::None) {
                rep.add(source, ctx, "'none' is not a valid entry in '" + std::string(key) + "'");
            } else {
                out.push_back(*parsed);
            }
        }
    };
    readList("weaknesses", a.weaknesses);
    readList("immunities", a.immunities);
    for (Element w : a.weaknesses) {
        if (a.immuneTo(w)) {
            rep.add(source, ctx,
                    "element '" + std::string(toString(w)) +
                        "' is listed in both 'weaknesses' and 'immunities'");
        }
    }
    return a;
}

// M45/M61: reads an optional status-rider list ({type, magnitude, duration}
// per entry) — `attackStatuses` on classes since M45 and bosses since M61 (the
// Deadly Duck), and since M75 also `initialStatuses` (battle-start statuses)
// on enemies and bosses. One reader so none of them can drift.
void readStatusList(const Json& el, const std::string& source, const std::string& ctx,
                    LoadReport& rep, const char* key, std::vector<AttackStatus>& out) {
    const auto it = el.find(key);
    if (it == el.end()) {
        return;
    }
    if (!it->is_array()) {
        rep.add(source, ctx + "." + key, "expected array");
        return;
    }
    int ai = 0;
    for (const auto& ae : *it) {
        const std::string actx = ctx + "." + key + "[" + std::to_string(ai) + "]";
        if (!ae.is_object()) {
            rep.add(source, actx, "expected object");
        } else {
            ObjectReader ar(ae, actx, source, rep);
            AttackStatus st;
            st.type = ar.reqEnum<StatusType>("type", parseStatusType, "status type");
            st.magnitude = ar.optIntMin("magnitude", 0, 0);
            st.duration = ar.reqIntMin("duration", 1);
            out.push_back(st);
        }
        ++ai;
    }
}

void readAttackStatuses(const Json& el, const std::string& source, const std::string& ctx,
                        LoadReport& rep, std::vector<AttackStatus>& out) {
    readStatusList(el, source, ctx, rep, "attackStatuses", out);
}

// M75: the optional `statusImmunities[]` list — statuses that can never land
// on this foe (the Dragon's bespoke matrix). `none` is rejected like the
// affinity reader's.
std::vector<StatusType> readStatusImmunities(ObjectReader& r, const std::string& source,
                                             const std::string& ctx, LoadReport& rep) {
    std::vector<StatusType> out;
    for (const std::string& name : r.optStringArray("statusImmunities")) {
        const std::optional<StatusType> parsed = parseStatusType(name);
        if (!parsed) {
            rep.add(source, ctx, "unknown status '" + name + "' in 'statusImmunities'");
        } else if (*parsed == StatusType::None) {
            rep.add(source, ctx, "'none' is not a valid entry in 'statusImmunities'");
        } else {
            out.push_back(*parsed);
        }
    }
    return out;
}

// M75: the optional `triggers[]` list (deterministic WHEN -> DO rules; see
// Definitions.hpp). One reader for enemies and bosses; `allowClone` is
// boss-only because the clone slot is prebuilt from the BossDef path.
void readTriggers(const Json& el, const std::string& source, const std::string& ctx,
                  LoadReport& rep, std::vector<TriggerDef>& out, bool allowClone) {
    const auto it = el.find("triggers");
    if (it == el.end()) {
        return;
    }
    if (!it->is_array()) {
        rep.add(source, ctx + ".triggers", "expected array");
        return;
    }
    int ti = 0;
    for (const auto& te : *it) {
        const std::string tctx = ctx + ".triggers[" + std::to_string(ti) + "]";
        ++ti;
        if (!te.is_object()) {
            rep.add(source, tctx, "expected object");
            continue;
        }
        ObjectReader tr(te, tctx, source, rep);
        TriggerDef t;
        t.when = tr.reqEnum<TriggerWhen>("when", parseTriggerWhen, "trigger condition");
        t.threshold = tr.optIntMin("threshold", 0, 0);
        t.action = tr.reqEnum<TriggerDo>("do", parseTriggerDo, "trigger action");
        t.status = tr.optEnum<StatusType>("status", parseStatusType, StatusType::None,
                                          "status type");
        t.magnitude = tr.optIntMin("magnitude", 0, 0);
        t.duration = tr.optIntMin("duration", 0, 0);
        t.scaleAttackPct = tr.optIntMin("scaleAttackPct", 100, 1);
        t.scaleMagicPct = tr.optIntMin("scaleMagicPct", 100, 1);
        t.scaleDefensePct = tr.optIntMin("scaleDefensePct", 100, 1);
        t.scaleSpeedPct = tr.optIntMin("scaleSpeedPct", 100, 1);
        t.cloneHpPct = tr.optIntMin("cloneHpPct", 0, 0);
        t.mpDrainPct = tr.optIntMin("mpDrainPct", 0, 0);
        t.text = tr.optString("text");

        // Semantic rules tying condition and action to their parameters.
        switch (t.when) {
            case TriggerWhen::EveryNthHitTaken:
            case TriggerWhen::EveryNthOwnTurn:
                if (t.threshold < 1) {
                    rep.add(source, tctx, "'threshold' must be >= 1 for this condition");
                }
                break;
            case TriggerWhen::FirstTimeHpBelowPct:
                if (t.threshold < 1 || t.threshold > 100) {
                    rep.add(source, tctx, "'threshold' must be 1..100 for this condition");
                }
                break;
            case TriggerWhen::FirstTimeAllyFelled:
            case TriggerWhen::None:
                break;
        }
        const bool statusAction = t.action == TriggerDo::StatusSelf ||
                                  t.action == TriggerDo::StatusAttacker ||
                                  t.action == TriggerDo::StatusAllFoes ||
                                  t.action == TriggerDo::StatusBoss;
        if (statusAction && (t.status == StatusType::None || t.duration < 1)) {
            rep.add(source, tctx,
                    "a status action requires a non-'none' 'status' and 'duration' >= 1");
        }
        if (t.action == TriggerDo::StatusAttacker && t.when != TriggerWhen::EveryNthHitTaken) {
            rep.add(source, tctx,
                    "'status_attacker' is only valid with 'every_nth_hit_taken'");
        }
        if (t.action == TriggerDo::ScaleStatsSelf) {
            const bool anyScale = t.scaleAttackPct != 100 || t.scaleMagicPct != 100 ||
                                  t.scaleDefensePct != 100 || t.scaleSpeedPct != 100;
            if (!anyScale) {
                rep.add(source, tctx, "'scale_stats_self' requires at least one scale percent");
            }
            const auto badScale = [](int v) { return v > 400; };
            if (badScale(t.scaleAttackPct) || badScale(t.scaleMagicPct) ||
                badScale(t.scaleDefensePct) || badScale(t.scaleSpeedPct)) {
                rep.add(source, tctx, "scale percents must be 1..400");
            }
        }
        if (t.action == TriggerDo::SummonCloneSelf) {
            if (!allowClone) {
                rep.add(source, tctx, "'summon_clone' is only valid on a boss");
            }
            if (t.cloneHpPct < 1 || t.cloneHpPct > 100) {
                rep.add(source, tctx, "'summon_clone' requires 'cloneHpPct' 1..100");
            }
        }
        if (t.action == TriggerDo::DrainFoeMp && (t.mpDrainPct < 1 || t.mpDrainPct > 100)) {
            rep.add(source, tctx, "'drain_foe_mp' requires 'mpDrainPct' 1..100");
        }
        out.push_back(std::move(t));
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
        d.reviveHpPct = r.optIntMin("reviveHpPct", 0, 0);  // M43 (default 0 = cannot revive)
        d.alsoBuffsEnemies = r.optBool("alsoBuffsEnemies", false);  // M45 (Goose tradeoff)
        d.mpDamagePct = r.optIntMin("mpDamagePct", 0, 0);  // M75 (MP damage rider)
        // M95: summons — once per run, announced by the creature's name.
        d.oncePerRun = r.optBool("oncePerRun", false);
        d.summonName = r.optString("summonName");
        if (!d.summonName.empty() && !d.oncePerRun) {
            rep.add(source, ctx, "'summonName' requires 'oncePerRun' (a summon is once per run)");
        }
        d.description = r.optString("description");

        // Semantic rules tying fields together (M43): a revive-capable skill is a
        // single-ally heal, and its percentage is a percentage.
        if (d.reviveHpPct > 100) {
            rep.add(source, ctx, "'reviveHpPct' must be 0..100");
        }
        if (d.reviveHpPct > 0 &&
            (d.category != SkillCategory::Heal || d.target != SkillTarget::SingleAlly)) {
            rep.add(source, ctx,
                    "'reviveHpPct' is only valid on a 'heal' skill targeting 'single_ally'");
        }
        // M75 semantic rules: MP damage rides a damaging skill; a Reflect
        // breaker must not be magic (it would bounce off the very mirror it
        // came to break) and aims at foes; an Uncurse aims at allies.
        if (d.mpDamagePct > 100) {
            rep.add(source, ctx, "'mpDamagePct' must be 0..100");
        }
        if (d.mpDamagePct > 0 && d.category != SkillCategory::Physical &&
            d.category != SkillCategory::Magic) {
            rep.add(source, ctx, "'mpDamagePct' is only valid on a physical or magic skill");
        }
        if (d.controlEffect == SkillEffect::BreakReflect) {
            if (d.category == SkillCategory::Magic) {
                rep.add(source, ctx, "'break_reflect' is not valid on a magic skill");
            }
            if (d.target != SkillTarget::SingleEnemy && d.target != SkillTarget::AllEnemies) {
                rep.add(source, ctx, "'break_reflect' must target enemies");
            }
        }
        if (d.controlEffect == SkillEffect::Uncurse && d.target != SkillTarget::SingleAlly &&
            d.target != SkillTarget::AllAllies && d.target != SkillTarget::Self) {
            rep.add(source, ctx, "'uncurse' must target allies or self");
        }
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
        // M45 reward-class fields: all optional, all inert by default.
        d.unlockedByKing = r.optBool("unlockedByKing", false);
        d.attackHitsAll = r.optBool("attackHitsAll", false);
        d.uncontrolled = r.optBool("uncontrolled", false);
        d.scoreModPct = r.optInt("scoreModPct", 0);
        for (const std::string& slot : r.optStringArray("equipBans")) {
            if (const std::optional<EquipSlot> parsed = parseEquipSlot(slot);
                parsed && *parsed != EquipSlot::None) {
                d.equipBans.push_back(*parsed);
            } else {
                rep.add(source, ctx, "unknown equip slot '" + slot + "' in 'equipBans'");
            }
        }
        readAttackStatuses(el, source, ctx, rep, d.attackStatuses);
        // Semantic rule: a score modifier is a percentage, not a multiplier.
        if (d.scoreModPct < -100 || d.scoreModPct > 100) {
            rep.add(source, ctx, "'scoreModPct' must be -100..100");
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
        d.passives = r.optStringArray("passives");  // M36 (optional)
        d.minTown = r.optIntMin("minTown", 1, 1);   // M38 (default 1)
        d.affinity = readAffinity(r, source, ctx, rep);  // M48 (optional)
        d.bossOnly = r.optBool("bossOnly", false);       // M49 (optional)
        d.doNothingPct = r.optIntMin("doNothingPct", 0, 0);  // M61 (the geese)
        d.doNothingText = r.optString("doNothingText");
        // M75: battle-start statuses, triggers, per-status immunities and the
        // sleep-aware AI manners — all optional, all inert by default.
        readStatusList(el, source, ctx, rep, "initialStatuses", d.initialStatuses);
        readTriggers(el, source, ctx, rep, d.triggers, /*allowClone=*/false);
        d.statusImmunities = readStatusImmunities(r, source, ctx, rep);
        d.avoidSleepingTargets = r.optBool("avoidSleepingTargets", false);
        d.noStunWhileAllFoesSleep = r.optBool("noStunWhileAllFoesSleep", false);
        d.maxMp = r.optIntMin("maxMp", 0, 0);  // M89 (0 = derive from magic)
        d.xpReward = r.optIntMin("xpReward", 0, 0);
        d.goldReward = r.optIntMin("goldReward", 0, 0);
        // M61 semantic rules: the chance is a percentage, and the flavour line
        // belongs to the roll that shows it.
        if (d.doNothingPct > 100) {
            rep.add(source, ctx, "'doNothingPct' must be 0..100");
        }
        if (!d.doNothingText.empty() && d.doNothingPct <= 0) {
            rep.add(source, ctx, "'doNothingText' requires 'doNothingPct' > 0");
        }
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
        d.element = r.optEnum<Element>("element", parseElement, Element::None, "element");  // M48
        d.value = r.optIntMin("value", 0, 0);
        d.minTown = r.optIntMin("minTown", 1, 1);  // M37 (default 1)
        d.maxTown = r.optIntMin("maxTown", 0, 0);  // M43 (default 0 = unbounded)
        d.maxHeld = r.optIntMin("maxHeld", 0, 0);           // M78 (0 = type default)
        d.notSoldInTown = r.optBool("notSoldInTown", false);  // M78 (premium tonics)
        d.effect = r.optEnum<ConsumableEffect>("effect", parseConsumableEffect,
                                               ConsumableEffect::None, "consumable effect");
        d.effectAmount = r.optIntMin("effectAmount", 0, 0);
        d.curesDebuffs = r.optBool("curesDebuffs", false);           // M43
        d.curesCurse = r.optBool("curesCurse", false);               // M75 (Holy Taxes)
        d.kingEffectAmount = r.optIntMin("kingEffectAmount", 0, 0);  // M43
        d.kingMpAmount = r.optIntMin("kingMpAmount", 0, 0);          // M43
        d.statBonus = r.optStatBlock("statBonus");
        // M75 (engine hook; content in M81): element resistance worn equipment
        // grants. `none` and unknown ids are rejected like the affinity lists.
        d.resistPct = r.optIntMin("resistPct", 0, 0);
        for (const std::string& name : r.optStringArray("resistElements")) {
            const std::optional<Element> parsed = parseElement(name);
            if (!parsed) {
                rep.add(source, ctx, "unknown element '" + name + "' in 'resistElements'");
            } else if (*parsed == Element::None) {
                rep.add(source, ctx, "'none' is not a valid entry in 'resistElements'");
            } else {
                d.resistElements.push_back(*parsed);
            }
        }
        d.iconCategory = r.optString("iconCategory");  // M81 (gear icons)
        d.useLine = r.optString("useLine");  // M76 (the duckling's punchline)
        d.grantsSkill = r.optString("grantsSkill");
        // M96 (rules v18): heirloom battle effects — the M75 trigger reader
        // (no clones on a worn keepsake) plus the conditional low-HP edge.
        readTriggers(el, source, ctx, rep, d.triggers, /*allowClone=*/false);
        d.lowHpThresholdPct = r.optIntMin("lowHpThresholdPct", 0, 0);
        d.lowHpAttackPct = r.optIntMin("lowHpAttackPct", 0, 0);
        if (d.type != ItemType::Heirloom &&
            (!d.triggers.empty() || d.lowHpThresholdPct > 0 || d.lowHpAttackPct > 0)) {
            rep.add(source, ctx, "trigger/lowHp fields are heirloom-only (M96)");
        }
        if ((d.lowHpThresholdPct > 0) != (d.lowHpAttackPct > 0)) {
            rep.add(source, ctx, "'lowHpThresholdPct' and 'lowHpAttackPct' come together");
        }
        if (d.lowHpThresholdPct > 100 || d.lowHpAttackPct > 100) {
            rep.add(source, ctx, "lowHp percents must be 0..100");
        }
        if (d.type == ItemType::Heirloom && d.slot != EquipSlot::Heirloom) {
            rep.add(source, ctx, "an heirloom's 'slot' must be 'heirloom'");
        }
        d.description = r.optString("description");
        // M44: enemy-targetable battle items, applied statuses, a boss
        // restriction, and a battle-long stat scale (the Royal Relics).
        d.battleTarget = r.optEnum<BattleTarget>("battleTarget", parseBattleTarget,
                                                 BattleTarget::Ally, "battle target");
        d.requiresBossId = r.optString("requiresBossId");
        d.statScalePct = r.optIntMin("statScalePct", 0, 0);
        d.disablesMinionRevive = r.optBool("disablesMinionRevive", false);  // M52
        if (const auto it = el.find("statuses"); it != el.end()) {
            if (!it->is_array()) {
                rep.add(source, ctx + ".statuses", "expected array");
            } else {
                int si = 0;
                for (const auto& se : *it) {
                    const std::string sctx = ctx + ".statuses[" + std::to_string(si) + "]";
                    if (!se.is_object()) {
                        rep.add(source, sctx, "expected object");
                    } else {
                        ObjectReader sr(se, sctx, source, rep);
                        ItemStatus st;
                        st.type = sr.reqEnum<StatusType>("type", parseStatusType, "status type");
                        st.magnitude = sr.optIntMin("magnitude", 0, 0);
                        st.duration = sr.reqIntMin("duration", 1);
                        d.statuses.push_back(st);
                    }
                    ++si;
                }
            }
        }

        // Semantic rules tying fields together.
        if (d.statScalePct > 100) {
            rep.add(source, ctx, "'statScalePct' must be 0..100 (it only weakens a target)");
        }
        if (d.maxTown > 0 && d.maxTown < d.minTown) {
            rep.add(source, ctx, "'maxTown' must be >= 'minTown' (or 0 for unbounded)");
        }
        if (d.type == ItemType::Scroll && d.grantsSkill.empty()) {
            rep.add(source, ctx, "scroll item must specify a non-empty 'grantsSkill'");
        }
        if ((d.type == ItemType::Equipment || d.type == ItemType::Relic) &&
            d.slot == EquipSlot::None) {
            rep.add(source, ctx, "equipment/relic must specify a non-'none' 'slot'");
        }
        // M48: only a weapon can carry an element — it is the wielder's basic
        // attack that gets it, so the field is meaningless anywhere else and an
        // armour piece claiming one is a content mistake worth catching.
        if (d.element != Element::None && d.slot != EquipSlot::Weapon) {
            rep.add(source, ctx, "'element' is only valid on a weapon (slot 'weapon')");
        }
        // M52: the revive-clock disable only means anything as an enemy-targeted
        // battle item (it acts on the foe it is used on), so flag a misplacement.
        if (d.disablesMinionRevive && d.battleTarget != BattleTarget::Enemy) {
            rep.add(source, ctx,
                    "'disablesMinionRevive' is only valid with battleTarget 'enemy'");
        }
        // M75: element resistance is worn, so it belongs to equipment/relics,
        // and the percent and the element list only mean anything together.
        if (d.resistPct > 100) {
            rep.add(source, ctx, "'resistPct' must be 0..100");
        }
        if ((d.resistPct > 0) != !d.resistElements.empty()) {
            rep.add(source, ctx,
                    "'resistPct' and 'resistElements' must be authored together");
        }
        if (d.resistPct > 0 && d.type != ItemType::Equipment && d.type != ItemType::Relic) {
            rep.add(source, ctx, "'resistPct' is only valid on equipment or a relic");
        }
        // M81: gear icons. The category must be one the shipped icon set
        // draws, only gear renders icons, and a weapon has no slot-derived
        // default (a sword and a staff share a slot) so it must author one.
        if (!d.iconCategory.empty()) {
            if (d.type != ItemType::Equipment && d.type != ItemType::Relic) {
                rep.add(source, ctx, "'iconCategory' is only valid on equipment or a relic");
            } else if (!isIconCategory(d.iconCategory)) {
                rep.add(source, ctx, "unknown 'iconCategory' '" + d.iconCategory + "'");
            }
        }
        if (d.type == ItemType::Equipment && d.slot == EquipSlot::Weapon &&
            iconCategoryFor(d).empty()) {
            rep.add(source, ctx,
                    "a weapon must author 'iconCategory' "
                    "(sword/axe/dagger/bow/staff/mace/spear)");
        }
        // M78: held-quantity caps and the town-shop delisting are consumable
        // policies; the cap ceiling is the owner's hard 9.
        if (d.maxHeld > 0 && d.type != ItemType::Consumable) {
            rep.add(source, ctx, "'maxHeld' is only valid on a consumable");
        }
        if (d.maxHeld > 9) {
            rep.add(source, ctx, "'maxHeld' must be 1..9 (the cap ceiling is 9)");
        }
        if (d.notSoldInTown && d.type != ItemType::Consumable) {
            rep.add(source, ctx, "'notSoldInTown' is only valid on a consumable");
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
        d.passives = r.optStringArray("passives");  // M36 (optional)
        d.minTown = r.optIntMin("minTown", 1, 1);   // M38 (default 1)
        d.affinity = readAffinity(r, source, ctx, rep);              // M48 (optional)
        d.reviveMinionTurns = r.optIntMin("reviveMinionTurns", 0, 0);  // M49 (0 = never)
        d.immuneToConfusion = r.optBool("immuneToConfusion", false);  // M40 (the King)
        d.attackHitsAll = r.optBool("attackHitsAll", false);          // M61 (the Duck)
        readAttackStatuses(el, source, ctx, rep, d.attackStatuses);   // M61 (the Duck)
        d.immuneToAfflictions = r.optBool("immuneToAfflictions", false);  // M61 (the Duck)
        // M75: battle-start statuses, triggers (clone allowed — boss-only),
        // per-status immunities, the sleep manners and the Deadly-Spoon shrug.
        readStatusList(el, source, ctx, rep, "initialStatuses", d.initialStatuses);
        readTriggers(el, source, ctx, rep, d.triggers, /*allowClone=*/true);
        d.statusImmunities = readStatusImmunities(r, source, ctx, rep);
        d.avoidSleepingTargets = r.optBool("avoidSleepingTargets", false);
        d.noStunWhileAllFoesSleep = r.optBool("noStunWhileAllFoesSleep", false);
        d.immuneToStatScale = r.optBool("immuneToStatScale", false);
        // M89: the optional MP pool override and the every-Nth-own-turn basic
        // attack (0 = off for both; the flavour line needs the rule on).
        d.maxMp = r.optIntMin("maxMp", 0, 0);
        d.basicAttackEveryNth = r.optIntMin("basicAttackEveryNth", 0, 0);
        d.basicAttackText = r.optString("basicAttackText");
        if (!d.basicAttackText.empty() && d.basicAttackEveryNth <= 0) {
            rep.add(source, ctx, "'basicAttackText' requires 'basicAttackEveryNth' > 0");
        }
        // M84: 0 = ordinary boss; 1..7 = that town's Guild Master. The upper
        // bound is the seven-town ladder (kTownCount; the story parser's 1..9
        // literal precedent).
        d.guildTown = r.optIntMin("guildTown", 0, 0);
        if (d.guildTown > 7) {
            rep.add(source, ctx, "'guildTown' must be 0 (ordinary boss) or a town 1..7");
        }
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

void parseStory(const Json& root, const std::string& source, ContentDatabase& db,
                LoadReport& rep) {
    forEachEntry(root, source, "story", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        StoryBeat d;
        // town 1..7 are the storyteller's installments; 8 (the castle) is the
        // Jester; 9 (M61, the Goose Town) is the Goofy Jester's duck tale;
        // 10 (M85) is the Pale Jester's Dragon introduction.
        d.town = r.reqIntRange("town", 1, 10);
        d.speaker = r.reqString("speaker");
        d.title = r.reqString("title");
        d.body = r.reqString("body");
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addStory(d)) {
            rep.add(source, ctx, "duplicate story beat for town " + std::to_string(d.town));
        }
    });
}

void parseEventFlavor(const Json& root, const std::string& source, ContentDatabase& db,
                      LoadReport& rep) {
    // M80: authored event flavor. Unknown ids are rejected so a typo cannot
    // silently author nothing; a rejected entry only costs its own panel (the
    // event falls back to the footer prompt).
    forEachEntry(root, source, "events", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        EventFlavorDef d;
        d.id = r.reqString("id");
        d.title = r.reqString("title");
        d.body = r.reqString("body");
        bool known = false;
        for (std::size_t i = 0; i < kEventFlavorIdCount; ++i) {
            if (d.id == kEventFlavorIds[i]) {
                known = true;
                break;
            }
        }
        if (!d.id.empty() && !known) {
            rep.add(source, ctx, "unknown event id '" + d.id + "'");
        }
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addEventFlavor(d)) {
            rep.add(source, ctx, "duplicate event id '" + d.id + "'");
        }
    });
}

void parseCurioLore(const Json& root, const std::string& source, ContentDatabase& db,
                    LoadReport& rep) {
    // M85: inspect-lore for the Maps screen's curios. The curio id table
    // lives a layer up (game/Curios.hpp), so KNOWN-ness and full coverage are
    // asserted by the [dragon] battery rather than here; the loader owns
    // shape and duplicates.
    forEachEntry(root, source, "curios", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        CurioLoreDef d;
        d.id = r.reqString("id");
        d.body = r.reqString("body");
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addCurioLore(d)) {
            rep.add(source, ctx, "duplicate curio id '" + d.id + "'");
        }
    });
}

void parseCutscenes(const Json& root, const std::string& source, ContentDatabase& db,
                    LoadReport& rep) {
    // M97: the Hooded Goose story scenes. Shape and vocabulary live here;
    // heirloom references are cross-file rules and live in validateReferences.
    forEachEntry(root, source, "cutscenes", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        CutsceneDef d;
        d.id = r.reqString("id");
        bool known = false;
        for (std::size_t i = 0; i < kCutsceneIdCount; ++i) {
            if (d.id == kCutsceneIds[i]) {
                known = true;
                break;
            }
        }
        if (!d.id.empty() && !known) {
            rep.add(source, ctx, "unknown cutscene id '" + d.id + "'");
        }
        d.question = r.reqString("question");

        const auto emoteKnown = [](const std::string& e) {
            for (std::size_t i = 0; i < kGooseEmoteCount; ++i) {
                if (e == kGooseEmotes[i]) {
                    return true;
                }
            }
            return false;
        };
        if (const auto beats = el.find("beats"); beats != el.end() && beats->is_array()) {
            int bi = 0;
            for (const auto& be : *beats) {
                const std::string bctx = ctx + ".beats[" + std::to_string(bi) + "]";
                ++bi;
                if (!be.is_object()) {
                    rep.add(source, bctx, "expected object");
                    continue;
                }
                ObjectReader br(be, bctx, source, rep);
                CutsceneBeat b;
                b.speaker = br.reqString("speaker");
                b.text = br.reqString("text");
                b.emote = br.optString("emote");
                if (b.emote.empty()) {
                    b.emote = "idle";
                } else if (!emoteKnown(b.emote)) {
                    rep.add(source, bctx, "unknown emote '" + b.emote + "'");
                }
                b.kingOnStage = br.optBool("kingOnStage", false);
                b.dragonOnStage = br.optBool("dragonOnStage", false);
                d.beats.push_back(std::move(b));
            }
        } else {
            rep.add(source, ctx, "'beats' array is required");
        }
        if (d.beats.empty()) {
            rep.add(source, ctx, "a cutscene needs at least one beat");
        }

        if (const auto opts = el.find("options"); opts != el.end() && opts->is_array()) {
            int oi = 0;
            for (const auto& oe : *opts) {
                const std::string octx = ctx + ".options[" + std::to_string(oi) + "]";
                ++oi;
                if (!oe.is_object()) {
                    rep.add(source, octx, "expected object");
                    continue;
                }
                ObjectReader orr(oe, octx, source, rep);
                CutsceneOption o;
                o.label = orr.reqString("label");
                o.heirloomId = orr.reqString("heirloom");
                o.responseSpeaker = orr.reqString("responseSpeaker");
                o.responseText = orr.reqString("responseText");
                d.options.push_back(std::move(o));
            }
        } else {
            rep.add(source, ctx, "'options' array is required");
        }
        // Exactly two: the choice UI, the heirloom set (8 scenes x 2 = 16),
        // and the skip rule ("the choice is never skippable") all assume it.
        if (d.options.size() != 2) {
            rep.add(source, ctx, "a cutscene needs exactly 2 options");
        }

        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addCutscene(d)) {
            rep.add(source, ctx, "duplicate cutscene id '" + d.id + "'");
        }
    });
}

void parseMilestones(const Json& root, const std::string& source, ContentDatabase& db,
                     LoadReport& rep) {
    // M63: class level-milestone bonuses. Tier and option are validated here;
    // classId references and a/b pair completeness are cross-entry rules and
    // live in validateReferences.
    forEachEntry(root, source, "milestones", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        MilestoneDef d;
        d.id = r.reqString("id");
        d.classId = r.reqString("classId");
        d.level = r.reqIntMin("level", 1);
        d.option = r.reqString("option");
        d.name = r.reqString("name");
        d.description = r.reqString("description");
        d.effect = r.reqEnum<MilestoneEffect>("effect", parseMilestoneEffect, "milestone effect");
        d.magnitude = r.optIntMin("magnitude", 0, 0);
        if (d.level != 10 && d.level != 20 && d.level != 30) {
            rep.add(source, ctx, "'level' must be 10, 20 or 30");
        }
        if (d.option != "a" && d.option != "b") {
            rep.add(source, ctx, "'option' must be 'a' or 'b'");
        }
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addMilestone(d)) {
            rep.add(source, ctx, "duplicate milestone id '" + d.id + "'");
        }
    });
}

void parsePassives(const Json& root, const std::string& source, ContentDatabase& db,
                   LoadReport& rep) {
    forEachEntry(root, source, "passives", rep, [&](const Json& el, const std::string& ctx, int) {
        const std::size_t before = rep.errorCount();
        ObjectReader r(el, ctx, source, rep);
        PassiveDef d;
        d.id = r.reqString("id");
        d.name = r.reqString("name");
        d.hook = r.reqEnum<PassiveHook>("hook", parsePassiveHook, "passive hook");
        d.magnitude = r.optIntMin("magnitude", 0, 0);
        d.price = r.optIntMin("price", 0, 0);
        d.description = r.optString("description");
        if (rep.errorCount() != before) {
            return;
        }
        if (!db.addPassive(d)) {
            rep.add(source, ctx, "duplicate passive id '" + d.id + "'");
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
        for (const auto& passive : enemy.passives) {
            if (!db.hasPassive(passive)) {
                rep.add(source, "enemy '" + id + "'.passives",
                        "references unknown passive '" + passive + "'");
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
        for (const auto& passive : boss.passives) {
            if (!db.hasPassive(passive)) {
                rep.add(source, "boss '" + id + "'.passives",
                        "references unknown passive '" + passive + "'");
            }
        }
    }
    // M84: a town keeps at most one Guild Master — two claimants would make
    // the gauntlet's team builder ambiguous.
    {
        std::array<std::string, 7> masters{};
        for (const auto& [id, boss] : db.bosses()) {
            if (boss.guildTown < 1 || boss.guildTown > 7) {
                continue;
            }
            std::string& slot = masters[static_cast<std::size_t>(boss.guildTown - 1)];
            if (!slot.empty()) {
                rep.add(source, "boss '" + id + "'",
                        "town " + std::to_string(boss.guildTown) +
                            " already has guild master '" + slot + "'");
            } else {
                slot = id;
            }
        }
    }
    // M63: milestones reference real classes and come in complete a/b pairs —
    // a lone option would leave the choice modal with nothing to choose.
    for (const auto& [id, m] : db.milestones()) {
        if (db.findClass(m.classId) == nullptr) {
            rep.add(source, "milestone '" + id + "'",
                    "references unknown class '" + m.classId + "'");
            continue;
        }
        const auto pair = db.milestonePair(m.classId, m.level);
        if (pair.first == nullptr || pair.second == nullptr) {
            rep.add(source, "milestone '" + id + "'",
                    "class '" + m.classId + "' level " + std::to_string(m.level) +
                        " needs exactly one 'a' and one 'b' option");
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
    // M97: cutscene options grant real heirlooms, and each heirloom belongs
    // to exactly one option anywhere — a keepsake granted twice would make
    // the story's 8x2 = 16 promise (and the replay no-re-grant rule) drift.
    {
        std::unordered_map<std::string, std::string> owners;  // heirloom -> scene
        for (const auto& [id, scene] : db.cutscenes()) {
            for (const auto& opt : scene.options) {
                const ItemDef* item = db.findItem(opt.heirloomId);
                if (item == nullptr) {
                    rep.add(source, "cutscene '" + id + "'.options",
                            "references unknown item '" + opt.heirloomId + "'");
                    continue;
                }
                if (item->type != ItemType::Heirloom) {
                    rep.add(source, "cutscene '" + id + "'.options",
                            "item '" + opt.heirloomId + "' is not an heirloom");
                    continue;
                }
                const auto [it, inserted] = owners.emplace(opt.heirloomId, id);
                if (!inserted) {
                    rep.add(source, "cutscene '" + id + "'.options",
                            "heirloom '" + opt.heirloomId + "' already granted by cutscene '" +
                                it->second + "'");
                }
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
    // Passives before enemies/bosses so their passive-id references validate.
    if (readJsonFile(dataRoot / "passives.json", json, rep)) {
        parsePassives(json, "passives.json", db, rep);
    }
    // M63: class level-milestone bonuses (classId refs checked in validateReferences).
    if (readJsonFile(dataRoot / "milestones.json", json, rep)) {
        parseMilestones(json, "milestones.json", db, rep);
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
    if (readJsonFile(dataRoot / "story.json", json, rep)) {
        parseStory(json, "story.json", db, rep);
    }
    // M97: the Hooded Goose cutscenes are REQUIRED (they grant heirlooms —
    // gameplay content, not the optional pure-presentation tier below).
    if (readJsonFile(dataRoot / "cutscenes.json", json, rep)) {
        parseCutscenes(json, "cutscenes.json", db, rep);
    }
    // M80: event flavor is an OPTIONAL content file — pure presentation
    // with a footer-prompt fallback per event, so its absence is not an error
    // and can never block play. Present-but-malformed is still reported like
    // any other file (a typo should be seen, not shrugged off).
    {
        std::error_code ec;
        if (fs::exists(dataRoot / "event_flavor.json", ec) && !ec) {
            if (readJsonFile(dataRoot / "event_flavor.json", json, rep)) {
                parseEventFlavor(json, "event_flavor.json", db, rep);
            }
        }
    }

    // M85: curio lore is the second optional file, on the same terms — a
    // curio without its entry simply shows name + description as before.
    {
        std::error_code ec;
        if (fs::exists(dataRoot / "curio_lore.json", ec) && !ec) {
            if (readJsonFile(dataRoot / "curio_lore.json", json, rep)) {
                parseCurioLore(json, "curio_lore.json", db, rep);
            }
        }
    }

    validateReferences(db, rep);
    return rep.errorCount() == before;
}

}  // namespace cd::content
