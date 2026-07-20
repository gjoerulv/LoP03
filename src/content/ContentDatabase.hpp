#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "content/Definitions.hpp"

// In-memory store of all loaded content, keyed by id. JSON-free so simulation
// code can depend on it without pulling in the loader. add* return false on a
// duplicate id (the loader turns that into a reported error).

namespace cd::content {

class ContentDatabase {
public:
    bool addSkill(const SkillDef& def);
    bool addClass(const ClassDef& def);
    bool addEnemy(const EnemyDef& def);
    bool addItem(const ItemDef& def);
    bool addBoss(const BossDef& def);
    bool addTheme(const DungeonThemeDef& def);

    const SkillDef* findSkill(const std::string& id) const;
    const ClassDef* findClass(const std::string& id) const;
    const EnemyDef* findEnemy(const std::string& id) const;
    const ItemDef* findItem(const std::string& id) const;
    const BossDef* findBoss(const std::string& id) const;
    const DungeonThemeDef* findTheme(const std::string& id) const;

    bool hasSkill(const std::string& id) const { return findSkill(id) != nullptr; }

    const std::unordered_map<std::string, SkillDef>& skills() const { return skills_; }
    const std::unordered_map<std::string, ClassDef>& classes() const { return classes_; }
    const std::unordered_map<std::string, EnemyDef>& enemies() const { return enemies_; }
    const std::unordered_map<std::string, ItemDef>& items() const { return items_; }
    const std::unordered_map<std::string, BossDef>& bosses() const { return bosses_; }
    const std::unordered_map<std::string, DungeonThemeDef>& themes() const { return themes_; }

    // Team-composition constraints (M20). Defaults apply until
    // data/composition.json is loaded.
    const CompositionDef& composition() const { return composition_; }
    void setComposition(const CompositionDef& c) { composition_ = c; }

    std::size_t skillCount() const { return skills_.size(); }
    std::size_t classCount() const { return classes_.size(); }
    std::size_t enemyCount() const { return enemies_.size(); }
    std::size_t itemCount() const { return items_.size(); }
    std::size_t bossCount() const { return bosses_.size(); }
    std::size_t themeCount() const { return themes_.size(); }

    bool empty() const;
    void clear();

private:
    std::unordered_map<std::string, SkillDef> skills_;
    std::unordered_map<std::string, ClassDef> classes_;
    std::unordered_map<std::string, EnemyDef> enemies_;
    std::unordered_map<std::string, ItemDef> items_;
    std::unordered_map<std::string, BossDef> bosses_;
    std::unordered_map<std::string, DungeonThemeDef> themes_;
    CompositionDef composition_;
};

}  // namespace cd::content
