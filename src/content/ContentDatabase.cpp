#include "content/ContentDatabase.hpp"

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

bool ContentDatabase::empty() const {
    return skills_.empty() && classes_.empty() && enemies_.empty() && items_.empty();
}

void ContentDatabase::clear() {
    skills_.clear();
    classes_.clear();
    enemies_.clear();
    items_.clear();
}

}  // namespace cd::content
