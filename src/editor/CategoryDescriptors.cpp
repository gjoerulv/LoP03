#include "editor/CategoryDescriptors.hpp"

#include "content/Definitions.hpp"
#include "content/Enums.hpp"

namespace cd::editor {

namespace {

// --- tiny builders so the tables below stay readable -----------------------

std::vector<std::string> ids(std::vector<std::string_view> views, bool dropNone = true) {
    std::vector<std::string> out;
    out.reserve(views.size());
    for (std::string_view v : views) {
        if (dropNone && v == "none") {
            continue;
        }
        out.emplace_back(v);
    }
    return out;
}

FieldDesc idField() {
    FieldDesc d;
    d.key = "id";
    d.label = "Id";
    d.kind = FieldKind::String;
    d.required = true;
    d.isId = true;
    return d;
}

FieldDesc str(const char* key, const char* label, bool required = false) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::String;
    d.required = required;
    return d;
}

FieldDesc txt(const char* key, const char* label, bool required = false) {
    FieldDesc d = str(key, label, required);
    d.kind = FieldKind::Text;
    return d;
}

FieldDesc num(const char* key, const char* label, double minV, double maxV, double step = 1.0,
              double def = 0.0, bool required = false) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::Int;
    d.required = required;
    d.minValue = minV;
    d.maxValue = maxV;
    d.step = step;
    d.defaultNumber = def;
    return d;
}

FieldDesc flt(const char* key, const char* label, double minV, double maxV, double step,
              double def = 0.0) {
    FieldDesc d = num(key, label, minV, maxV, step, def);
    d.kind = FieldKind::Float;
    return d;
}

FieldDesc bl(const char* key, const char* label) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::Bool;
    return d;
}

FieldDesc en(const char* key, const char* label, std::vector<std::string> values,
             const char* def = "none", bool required = false) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::Enum;
    d.required = required;
    d.enumValues = std::move(values);
    d.defaultString = def;
    return d;
}

FieldDesc ref(const char* key, const char* label, Category target) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::IdRef;
    d.refCategory = target;
    return d;
}

FieldDesc refList(const char* key, const char* label, Category target, bool required = false) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::IdList;
    d.required = required;
    d.refCategory = target;
    return d;
}

FieldDesc enList(const char* key, const char* label, std::vector<std::string> values) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::EnumList;
    d.enumValues = std::move(values);
    return d;
}

FieldDesc obj(const char* key, const char* label, std::vector<FieldDesc> children,
              bool required = false) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::Object;
    d.required = required;
    d.children = std::move(children);
    return d;
}

FieldDesc objArr(const char* key, const char* label, std::vector<FieldDesc> children) {
    FieldDesc d;
    d.key = key;
    d.label = label;
    d.kind = FieldKind::ObjectArray;
    d.children = std::move(children);
    return d;
}

// Shared sub-schemas. StatBlock JSON keys ("hp" -> maxHp) per JsonValidation's
// optStatBlock; growth mirrors them as floats.
std::vector<FieldDesc> statChildren(int hpMin, int otherMin) {
    return {
        num("hp", "HP", hpMin, 99999),
        num("attack", "ATK", otherMin, 9999),
        num("magic", "MAG", otherMin, 9999),
        num("defense", "DEF", otherMin, 9999),
        num("speed", "SPD", otherMin, 9999),
    };
}

std::vector<FieldDesc> growthChildren() {
    return {
        flt("hp", "HP / level", 0.0, 99.0, 0.5),
        flt("attack", "ATK / level", 0.0, 99.0, 0.1),
        flt("magic", "MAG / level", 0.0, 99.0, 0.1),
        flt("defense", "DEF / level", 0.0, 99.0, 0.1),
        flt("speed", "SPD / level", 0.0, 99.0, 0.1),
    };
}

std::vector<FieldDesc> statusRiderChildren() {
    return {
        en("type", "Status", ids(content::statusTypeIds()), "none", true),
        num("magnitude", "Magnitude", 0, 300),
        num("duration", "Duration", 0, 99),
    };
}

// M75: one deterministic WHEN -> DO trigger row (enemies and bosses).
std::vector<FieldDesc> triggerChildren() {
    return {
        en("when", "When", ids(content::triggerWhenIds()), "first_time_hp_below_pct", true),
        num("threshold", "Threshold (N / HP %)", 0, 100),
        en("do", "Do", ids(content::triggerDoIds()), "status_self", true),
        en("status", "Status", ids(content::statusTypeIds())),
        num("magnitude", "Magnitude", 0, 300),
        num("duration", "Duration", 0, 99),
        num("scaleAttackPct", "Scale ATK %", 1, 400, 5, 100),
        num("scaleMagicPct", "Scale MAG %", 1, 400, 5, 100),
        num("scaleDefensePct", "Scale DEF %", 1, 400, 5, 100),
        num("scaleSpeedPct", "Scale SPD %", 1, 400, 5, 100),
        num("cloneHpPct", "Clone HP %", 0, 100),
        num("mpDrainPct", "MP Drain %", 0, 100),
        str("text", "Announcement"),
    };
}

// --- per-category tables ----------------------------------------------------

std::vector<FieldDesc> skillDescs() {
    return {
        idField(),
        str("name", "Name", true),
        en("category", "Category", ids(content::skillCategoryIds()), "physical", true),
        en("target", "Target", ids(content::skillTargetIds()), "single_enemy", true),
        en("element", "Element", ids(content::elementIds())),
        num("power", "Power", 0, 999),
        num("mpCost", "MP Cost", 0, 99),
        en("statusEffect", "Status Effect", ids(content::statusTypeIds())),
        num("statusMagnitude", "Status Magnitude", 0, 300),
        num("statusDuration", "Status Duration", 0, 99),
        // The loader's key is "control" (the Def field is controlEffect).
        en("control", "Control Effect", ids(content::skillEffectIds())),
        num("reviveHpPct", "Revive HP %", 0, 100),
        bl("alsoBuffsEnemies", "Also Buffs Enemies"),
        num("mpDamagePct", "MP Damage % (M75)", 0, 100),
        bl("oncePerRun", "Once Per Run (M95 summon)"),
        str("summonName", "Summon Name (M95)"),
        txt("description", "Description"),
    };
}

std::vector<FieldDesc> classDescs() {
    return {
        idField(),
        str("name", "Name", true),
        str("role", "Role"),
        obj("baseStats", "Base Stats", statChildren(1, 0), true),
        obj("growth", "Growth", growthChildren()),
        refList("startingSkills", "Starting Skills", Category::Skills),
        objArr("learnset", "Learnset",
               {
                   ref("skill", "Skill", Category::Skills),
                   num("level", "Level", 1, 99, 1, 1, true),
               }),
        bl("unlockedByKing", "Unlocked By King"),
        enList("equipBans", "Equip Bans", ids(content::equipSlotIds())),
        bl("attackHitsAll", "Attack Hits All"),
        objArr("attackStatuses", "Attack Statuses", statusRiderChildren()),
        bl("uncontrolled", "Uncontrolled"),
        num("scoreModPct", "Score Mod %", -99, 99),
    };
}

std::vector<FieldDesc> enemyDescs() {
    return {
        idField(),
        str("name", "Name", true),
        obj("stats", "Stats", statChildren(1, 0), true),
        en("tier", "Tier", ids(content::enemyTierIds()), "normal"),
        en("role", "Role", ids(content::enemyRoleIds()), "bruiser", true),
        enList("tags", "Tags", ids(content::enemyTagIds())),
        refList("skills", "Skills", Category::Skills),
        refList("passives", "Passives", Category::Passives),
        num("minTown", "Min Town", 1, 7, 1, 1),
        enList("weaknesses", "Weak To", ids(content::elementIds())),
        enList("immunities", "Immune To", ids(content::elementIds())),
        bl("bossOnly", "Boss Only"),
        num("doNothingPct", "Do-Nothing % (M61)", 0, 100),
        str("doNothingText", "Do-Nothing Line"),
        objArr("initialStatuses", "Initial Statuses (M75)", statusRiderChildren()),
        objArr("triggers", "Triggers (M75)", triggerChildren()),
        enList("statusImmunities", "Status Immunities", ids(content::statusTypeIds())),
        bl("avoidSleepingTargets", "Avoid Sleeping Targets"),
        bl("noStunWhileAllFoesSleep", "No Stun While All Sleep"),
        num("maxMp", "Max MP Override (M89)", 0, 9999),
        num("xpReward", "XP Reward", 0, 99999),
        num("goldReward", "Gold Reward", 0, 99999),
    };
}

std::vector<FieldDesc> bossDescs() {
    return {
        idField(),
        str("name", "Name", true),
        // reqEnum in the loader: the key must always be written.
        en("archetype", "Archetype", ids(content::bossArchetypeIds()), "brute", true),
        obj("stats", "Stats", statChildren(1, 0), true),
        refList("skills", "Skills", Category::Skills),
        refList("minions", "Minions", Category::Enemies),
        refList("passives", "Passives", Category::Passives),
        num("minTown", "Min Town", 1, 7, 1, 1),
        // M84: 0 = ordinary boss; 1..7 marks the town's Guild Master (fought
        // only in that town's guild gauntlet, excluded from dungeons/rush).
        num("guildTown", "Guild Master Of Town (M84)", 0, 7),
        enList("weaknesses", "Weak To", ids(content::elementIds())),
        enList("immunities", "Immune To", ids(content::elementIds())),
        num("reviveMinionTurns", "Revive Minions (turns)", 0, 99),
        bl("immuneToConfusion", "Immune To Confusion"),
        bl("attackHitsAll", "Attack Hits All (M61)"),
        objArr("attackStatuses", "Attack Statuses", statusRiderChildren()),
        bl("immuneToAfflictions", "Immune To Afflictions"),
        objArr("initialStatuses", "Initial Statuses (M75)", statusRiderChildren()),
        objArr("triggers", "Triggers (M75)", triggerChildren()),
        enList("statusImmunities", "Status Immunities", ids(content::statusTypeIds())),
        bl("avoidSleepingTargets", "Avoid Sleeping Targets"),
        bl("noStunWhileAllFoesSleep", "No Stun While All Sleep"),
        bl("immuneToStatScale", "Immune To Stat Scale (Spoon)"),
        num("maxMp", "Max MP Override (M89)", 0, 9999),
        num("basicAttackEveryNth", "Basic Attack Every Nth (M89)", 0, 99),
        str("basicAttackText", "Basic Attack Line (M89)"),
        txt("telegraph", "Telegraph"),
        num("xpReward", "XP Reward", 0, 99999),
        num("goldReward", "Gold Reward", 0, 99999),
        txt("description", "Description"),
    };
}

std::vector<FieldDesc> itemDescs() {
    return {
        idField(),
        str("name", "Name", true),
        // reqEnum in the loader: the key must always be written.
        en("type", "Type", ids(content::itemTypeIds()), "consumable", true),
        en("slot", "Slot", ids(content::equipSlotIds())),
        en("rarity", "Rarity", ids(content::rarityIds()), "common"),
        en("element", "Element", ids(content::elementIds())),
        num("value", "Value (gold)", 0, 99999, 10),
        num("minTown", "Min Town", 1, 7, 1, 1),
        num("maxTown", "Max Town", 0, 7),
        num("maxHeld", "Max Held (M78)", 0, 9),
        bl("notSoldInTown", "Not Sold In Town (M78)"),
        en("effect", "Effect", ids(content::consumableEffectIds())),
        num("effectAmount", "Effect Amount", 0, 9999),
        bl("curesDebuffs", "Cures Debuffs"),
        bl("curesCurse", "Cures Curse (M75)"),
        num("kingEffectAmount", "King Effect Amount", 0, 9999),
        num("kingMpAmount", "King MP Amount", 0, 9999),
        en("battleTarget", "Battle Target", ids(content::battleTargetIds()), "ally"),
        objArr("statuses", "Statuses", statusRiderChildren()),
        ref("requiresBossId", "Requires Boss", Category::Bosses),
        num("statScalePct", "Stat Scale %", 0, 200),
        bl("disablesMinionRevive", "Disables Minion Revive"),
        obj("statBonus", "Stat Bonus", statChildren(-99, -99)),
        enList("resistElements", "Resist Elements (M75)", ids(content::elementIds())),
        num("resistPct", "Resist %", 0, 100),
        en("iconCategory", "Icon Category (M81)", ids(content::iconCategoryIds()), ""),
        str("useLine", "Use Line (M76)"),
        ref("grantsSkill", "Grants Skill (scroll)", Category::Skills),
        objArr("triggers", "Triggers (M96 heirloom)", triggerChildren()),
        num("lowHpThresholdPct", "Low-HP Threshold % (M96)", 0, 100),
        num("lowHpAttackPct", "Low-HP Attack Bonus % (M96)", 0, 100),
        txt("description", "Description"),
    };
}

std::vector<FieldDesc> passiveDescs() {
    return {
        idField(),
        str("name", "Name", true),
        en("hook", "Hook", ids(content::passiveHookIds()), "counter", true),
        num("magnitude", "Magnitude", 0, 300),
        num("price", "Price (gold)", 0, 99999, 50),
        txt("description", "Description"),
    };
}

std::vector<FieldDesc> milestoneDescs() {
    // M63 class level-milestone bonuses. Every key is loader-required except
    // magnitude (optIntMin default 0, omitted at 0 like the sparse style).
    FieldDesc classRef = ref("classId", "Class", Category::Classes);
    classRef.required = true;  // reqString in the loader: always written
    return {
        idField(),
        classRef,
        num("level", "Level (10/20/30)", 10, 30, 10, 10, true),
        en("option", "Option", {"a", "b"}, "a", true),
        str("name", "Name", true),
        txt("description", "Description", true),
        en("effect", "Effect", ids(content::milestoneEffectIds()), "stat_max_hp_pct", true),
        num("magnitude", "Magnitude", 0, 300),
    };
}

std::vector<FieldDesc> themeDescs() {
    return {
        idField(),
        str("name", "Name", true),
        refList("normalEnemies", "Normal Enemies", Category::Enemies, true),
        refList("eliteEnemies", "Elite Enemies", Category::Enemies, true),
        refList("bosses", "Bosses", Category::Bosses, true),
        txt("description", "Description"),
    };
}

std::vector<FieldDesc> compositionDescs() {
    return {
        num("version", "Schema Version", 1, 1, 1, 1, true),
        obj("team", "Team Rules",
            {
                num("minSize", "Min Size", 1, 9, 1, 2, true),
                num("deepMinSize", "Deep Min Size", 1, 9, 1, 3, true),
                num("deepMinDepth", "Deep Min Depth", 1, 99, 1, 4, true),
                num("maxSizeBase", "Max Size Base", 2, 9, 1, 2, true),
                num("maxSizePerDepths", "Max Size / Depths", 1, 99, 1, 2, true),
                num("maxSizeCap", "Max Size Cap", 2, 9, 1, 5, true),
                num("elitePctPerDepth", "Elite % / Depth", 0, 100, 1, 9, true),
                num("elitePctMax", "Elite % Max", 0, 100, 1, 70, true),
                num("maxSupport", "Max Support", 0, 9, 1, 1, true),
                num("minDamage", "Min Damage", 0, 9, 1, 1, true),
            },
            true),
        obj("boss", "Boss Rules",
            {
                num("minMinions", "Min Minions", 0, 9, 1, 0, true),
                num("maxMinions", "Max Minions", 0, 9, 1, 3, true),
            },
            true),
        obj("statScale", "Stat Scaling",
            {
                num("startDepth", "Start Depth", 1, 99, 1, 5, true),
                num("pctPerDepth", "% / Depth", 0, 100, 1, 6, true),
                num("pctMax", "% Max", 0, 300, 1, 90, true),
            },
            true),
    };
}

std::vector<FieldDesc> storyDescs() {
    // Town bounds follow the loader (M85 widened 9 -> 10 for the Pale Jester).
    return {
        num("town", "Town (8 castle, 9 goose, 10 pale jester)", 1, 10, 1, 1, true),
        str("speaker", "Speaker", true),
        str("title", "Title", true),
        txt("body", "Body", true),
    };
}

// M86: the two optional content files (M80 event flavor, M85 curio lore).
// Both are plain id-keyed entity arrays; the loader owns shape/duplicates and
// (for events) rejects unknown ids, so a typo'd id fails validation on save
// rather than silently authoring nothing.
std::vector<FieldDesc> eventFlavorDescs() {
    return {
        idField(),
        str("title", "Title", true),
        txt("body", "Body", true),
    };
}

std::vector<FieldDesc> curioLoreDescs() {
    return {
        idField(),
        txt("body", "Body", true),
    };
}

// M97: one scene = dialogue beats (modal-edited rows) + the mandatory
// two-option choice. Emotes follow content::kGooseEmotes; each option's
// heirloom is an Items reference (existence, heirloom-ness, and one-owner
// uniqueness are validateReferences rules, reported on save).
std::vector<FieldDesc> cutsceneDescs() {
    std::vector<std::string> emotes;
    for (std::size_t i = 0; i < content::kGooseEmoteCount; ++i) {
        emotes.emplace_back(content::kGooseEmotes[i]);
    }
    return {
        idField(),
        str("question", "Choice Question", true),
        objArr("beats", "Beats",
               {str("speaker", "Speaker", true), txt("text", "Text", true),
                en("emote", "Goose Emote", emotes, "idle"),
                bl("kingOnStage", "King On Stage"), bl("dragonOnStage", "Dragon On Stage")}),
        objArr("options", "Choice Options (exactly 2)",
               {str("label", "Label", true), ref("heirloom", "Heirloom", Category::Items),
                str("responseSpeaker", "Response Speaker", true),
                txt("responseText", "Response Text", true)}),
    };
}

}  // namespace

const std::vector<CategoryInfo>& categories() {
    static const std::vector<CategoryInfo> kInfos = {
        {Category::Skills, "skills.json", "skills", "Skills", true},
        {Category::Classes, "classes.json", "classes", "Classes", true},
        {Category::Enemies, "enemies.json", "enemies", "Enemies", true},
        {Category::Bosses, "bosses.json", "bosses", "Bosses", true},
        {Category::Items, "items.json", "items", "Items", true},
        {Category::Passives, "passives.json", "passives", "Passives", true},
        {Category::Milestones, "milestones.json", "milestones", "Milestones", true},
        {Category::Themes, "dungeon_themes.json", "themes", "Themes", true},
        {Category::Composition, "composition.json", "", "Composition", false},
        {Category::Story, "story.json", "story", "Story", false},
        {Category::EventFlavor, "event_flavor.json", "events", "Event Flavor", true},
        {Category::CurioLore, "curio_lore.json", "curios", "Curio Lore", true},
        {Category::Cutscenes, "cutscenes.json", "cutscenes", "Cutscenes", true},  // M97
    };
    return kInfos;
}

const CategoryInfo& infoFor(Category category) {
    for (const CategoryInfo& info : categories()) {
        if (info.category == category) {
            return info;
        }
    }
    return categories().front();
}

const std::vector<FieldDesc>& descriptorsFor(Category category) {
    static const std::vector<FieldDesc> kSkills = skillDescs();
    static const std::vector<FieldDesc> kClasses = classDescs();
    static const std::vector<FieldDesc> kEnemies = enemyDescs();
    static const std::vector<FieldDesc> kBosses = bossDescs();
    static const std::vector<FieldDesc> kItems = itemDescs();
    static const std::vector<FieldDesc> kPassives = passiveDescs();
    static const std::vector<FieldDesc> kMilestones = milestoneDescs();
    static const std::vector<FieldDesc> kThemes = themeDescs();
    static const std::vector<FieldDesc> kComposition = compositionDescs();
    static const std::vector<FieldDesc> kStory = storyDescs();
    static const std::vector<FieldDesc> kEventFlavor = eventFlavorDescs();
    static const std::vector<FieldDesc> kCurioLore = curioLoreDescs();
    static const std::vector<FieldDesc> kCutscenes = cutsceneDescs();
    switch (category) {
        case Category::Skills: return kSkills;
        case Category::Classes: return kClasses;
        case Category::Enemies: return kEnemies;
        case Category::Bosses: return kBosses;
        case Category::Items: return kItems;
        case Category::Passives: return kPassives;
        case Category::Milestones: return kMilestones;
        case Category::Themes: return kThemes;
        case Category::Composition: return kComposition;
        case Category::Story: return kStory;
        case Category::EventFlavor: return kEventFlavor;
        case Category::CurioLore: return kCurioLore;
        case Category::Cutscenes: return kCutscenes;
    }
    return kSkills;
}

}  // namespace cd::editor
