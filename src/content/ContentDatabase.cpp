#include "content/ContentDatabase.hpp"

#include <algorithm>

namespace cd::content {

namespace {

template <typename Map, typename Def>
bool insertUnique(Map& map, const Def& def) {
    return map.emplace(def.id, def).second;  // false if id already present
}

template <typename Map>
auto findIn(const Map& map, const std::string& id) -> const typename Map::mapped_type* {
    auto it = map.find(id);
    return it == map.end() ? nullptr : &it->second;
}

}  // namespace

bool ContentDatabase::addSkill(const SkillDef& def) { return insertUnique(skills_, def); }
bool ContentDatabase::addClass(const ClassDef& def) { return insertUnique(classes_, def); }
bool ContentDatabase::addEnemy(const EnemyDef& def) { return insertUnique(enemies_, def); }
bool ContentDatabase::addItem(const ItemDef& def) { return insertUnique(items_, def); }
bool ContentDatabase::addBoss(const BossDef& def) { return insertUnique(bosses_, def); }
bool ContentDatabase::addTheme(const DungeonThemeDef& def) { return insertUnique(themes_, def); }
bool ContentDatabase::addPassive(const PassiveDef& def) { return insertUnique(passives_, def); }
bool ContentDatabase::addMilestone(const MilestoneDef& def) {
    return insertUnique(milestones_, def);  // M63
}
bool ContentDatabase::addStory(const StoryBeat& def) {
    for (const StoryBeat& b : story_) {
        if (b.town == def.town) {
            return false;  // one beat per town
        }
    }
    story_.push_back(def);
    return true;
}

const SkillDef* ContentDatabase::findSkill(const std::string& id) const {
    return findIn(skills_, id);
}
const ClassDef* ContentDatabase::findClass(const std::string& id) const {
    return findIn(classes_, id);
}
const EnemyDef* ContentDatabase::findEnemy(const std::string& id) const {
    return findIn(enemies_, id);
}
const ItemDef* ContentDatabase::findItem(const std::string& id) const {
    return findIn(items_, id);
}
const BossDef* ContentDatabase::findBoss(const std::string& id) const {
    return findIn(bosses_, id);
}
const DungeonThemeDef* ContentDatabase::findTheme(const std::string& id) const {
    return findIn(themes_, id);
}
const PassiveDef* ContentDatabase::findPassive(const std::string& id) const {
    return findIn(passives_, id);
}
const MilestoneDef* ContentDatabase::findMilestone(const std::string& id) const {
    return findIn(milestones_, id);  // M63
}
std::pair<const MilestoneDef*, const MilestoneDef*> ContentDatabase::milestonePair(
    const std::string& classId, int level) const {
    std::pair<const MilestoneDef*, const MilestoneDef*> out{nullptr, nullptr};
    for (const auto& [id, m] : milestones_) {
        if (m.classId != classId || m.level != level) {
            continue;
        }
        (m.option == "a" ? out.first : out.second) = &m;
    }
    return out;
}
const StoryBeat* ContentDatabase::findStoryBeat(int town) const {
    for (const StoryBeat& b : story_) {
        if (b.town == town) {
            return &b;
        }
    }
    return nullptr;
}

bool ContentDatabase::addEventFlavor(const EventFlavorDef& def) {
    return eventFlavors_.emplace(def.id, def).second;
}

const EventFlavorDef* ContentDatabase::findEventFlavor(const std::string& id) const {
    return findIn(eventFlavors_, id);
}

bool ContentDatabase::addCurioLore(const CurioLoreDef& def) {  // M85
    return curioLores_.emplace(def.id, def).second;
}

const CurioLoreDef* ContentDatabase::findCurioLore(const std::string& id) const {  // M85
    return findIn(curioLores_, id);
}

bool ContentDatabase::addCutscene(const CutsceneDef& def) {  // M97
    return cutscenes_.emplace(def.id, def).second;
}

const CutsceneDef* ContentDatabase::findCutscene(const std::string& id) const {  // M97
    return findIn(cutscenes_, id);
}

bool ContentDatabase::empty() const {
    return skills_.empty() && classes_.empty() && enemies_.empty() && items_.empty() &&
           bosses_.empty() && themes_.empty() && passives_.empty();
}

void ContentDatabase::clear() {
    skills_.clear();
    classes_.clear();
    enemies_.clear();
    items_.clear();
    bosses_.clear();
    themes_.clear();
    passives_.clear();
    story_.clear();
    eventFlavors_.clear();  // M80
    curioLores_.clear();  // M85
    cutscenes_.clear();  // M97
}

std::vector<std::string> knownSkillsFor(const ClassDef& cls, int level) {
    const int lv = level < 1 ? 1 : level;
    std::vector<std::string> result = cls.startingSkills;  // level-1 set, order kept

    // Learnset entries available at this level, stably ordered by ascending
    // level (ties keep declaration order via stable_sort).
    std::vector<const LearnEntry*> available;
    for (const LearnEntry& e : cls.learnset) {
        if (e.level <= lv) {
            available.push_back(&e);
        }
    }
    std::stable_sort(available.begin(), available.end(),
                     [](const LearnEntry* a, const LearnEntry* b) { return a->level < b->level; });
    for (const LearnEntry* e : available) {
        if (std::find(result.begin(), result.end(), e->skill) == result.end()) {
            result.push_back(e->skill);
        }
    }
    return result;
}

}  // namespace cd::content
