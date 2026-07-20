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

bool ContentDatabase::empty() const {
    return skills_.empty() && classes_.empty() && enemies_.empty() && items_.empty() &&
           bosses_.empty() && themes_.empty();
}

void ContentDatabase::clear() {
    skills_.clear();
    classes_.clear();
    enemies_.clear();
    items_.clear();
    bosses_.clear();
    themes_.clear();
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
