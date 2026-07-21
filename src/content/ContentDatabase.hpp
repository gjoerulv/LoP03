#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

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
    bool addPassive(const PassiveDef& def);
    bool addStory(const StoryBeat& def);  // M41; false on a duplicate town

    const SkillDef* findSkill(const std::string& id) const;
    const ClassDef* findClass(const std::string& id) const;
    const EnemyDef* findEnemy(const std::string& id) const;
    const ItemDef* findItem(const std::string& id) const;
    const BossDef* findBoss(const std::string& id) const;
    const DungeonThemeDef* findTheme(const std::string& id) const;
    const PassiveDef* findPassive(const std::string& id) const;
    const StoryBeat* findStoryBeat(int town) const;  // M41

    bool hasSkill(const std::string& id) const { return findSkill(id) != nullptr; }
    bool hasPassive(const std::string& id) const { return findPassive(id) != nullptr; }

    const std::unordered_map<std::string, SkillDef>& skills() const { return skills_; }
    const std::unordered_map<std::string, ClassDef>& classes() const { return classes_; }
    const std::unordered_map<std::string, EnemyDef>& enemies() const { return enemies_; }
    const std::unordered_map<std::string, ItemDef>& items() const { return items_; }
    const std::unordered_map<std::string, BossDef>& bosses() const { return bosses_; }
    const std::unordered_map<std::string, DungeonThemeDef>& themes() const { return themes_; }
    const std::unordered_map<std::string, PassiveDef>& passives() const { return passives_; }
    const std::vector<StoryBeat>& story() const { return story_; }  // M41

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
    std::size_t passiveCount() const { return passives_.size(); }
    std::size_t storyCount() const { return story_.size(); }

    bool empty() const;
    void clear();

private:
    std::unordered_map<std::string, SkillDef> skills_;
    std::unordered_map<std::string, ClassDef> classes_;
    std::unordered_map<std::string, EnemyDef> enemies_;
    std::unordered_map<std::string, ItemDef> items_;
    std::unordered_map<std::string, BossDef> bosses_;
    std::unordered_map<std::string, DungeonThemeDef> themes_;
    std::unordered_map<std::string, PassiveDef> passives_;
    std::vector<StoryBeat> story_;
    CompositionDef composition_;
};

// Skills a character of `level` knows: the class's `startingSkills` (the level-1
// set) plus every learnset entry whose `level` is <= the character's level, in a
// stable order (startingSkills first, then learnset by ascending level then
// declaration order), de-duplicated. Pure and level-deterministic — the single
// source of a party member's usable skills (M29). Because skills are derived,
// not stored, this keeps the simulator and live play in exact agreement and
// requires no save state.
std::vector<std::string> knownSkillsFor(const ClassDef& cls, int level);

}  // namespace cd::content
