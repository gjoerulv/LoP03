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
    bool addMilestone(const MilestoneDef& def);  // M63
    bool addStory(const StoryBeat& def);  // M41; false on a duplicate town
    bool addEventFlavor(const EventFlavorDef& def);  // M80
    bool addCurioLore(const CurioLoreDef& def);  // M85
    bool addCutscene(const CutsceneDef& def);  // M97

    const SkillDef* findSkill(const std::string& id) const;
    const ClassDef* findClass(const std::string& id) const;
    const EnemyDef* findEnemy(const std::string& id) const;
    const ItemDef* findItem(const std::string& id) const;
    const BossDef* findBoss(const std::string& id) const;
    const DungeonThemeDef* findTheme(const std::string& id) const;
    const PassiveDef* findPassive(const std::string& id) const;
    const MilestoneDef* findMilestone(const std::string& id) const;  // M63
    // M63: the a/b pair for a class at a tier ({nullptr, nullptr} when the
    // class has none). Deterministic regardless of map order — matched by the
    // validated `option` field, never by iteration.
    std::pair<const MilestoneDef*, const MilestoneDef*> milestonePair(const std::string& classId,
                                                                      int level) const;
    const StoryBeat* findStoryBeat(int town) const;  // M41
    // M80: nullptr when unauthored — the caller falls back to the footer
    // prompt, so flavor can never block an event.
    const EventFlavorDef* findEventFlavor(const std::string& id) const;
    // M85: nullptr when unauthored — the Maps screen falls back to the
    // curio's own name + description, so lore can never block the panel.
    const CurioLoreDef* findCurioLore(const std::string& id) const;
    // M97: nullptr when the scene is unauthored — every trigger site checks
    // first, so a missing scene simply never plays (no crash, no block).
    const CutsceneDef* findCutscene(const std::string& id) const;

    bool hasSkill(const std::string& id) const { return findSkill(id) != nullptr; }
    bool hasPassive(const std::string& id) const { return findPassive(id) != nullptr; }

    const std::unordered_map<std::string, SkillDef>& skills() const { return skills_; }
    const std::unordered_map<std::string, ClassDef>& classes() const { return classes_; }
    const std::unordered_map<std::string, EnemyDef>& enemies() const { return enemies_; }
    const std::unordered_map<std::string, ItemDef>& items() const { return items_; }
    const std::unordered_map<std::string, BossDef>& bosses() const { return bosses_; }
    const std::unordered_map<std::string, DungeonThemeDef>& themes() const { return themes_; }
    const std::unordered_map<std::string, PassiveDef>& passives() const { return passives_; }
    const std::unordered_map<std::string, MilestoneDef>& milestones() const { return milestones_; }
    const std::vector<StoryBeat>& story() const { return story_; }  // M41
    const std::unordered_map<std::string, EventFlavorDef>& eventFlavors() const {
        return eventFlavors_;  // M80
    }
    const std::unordered_map<std::string, CurioLoreDef>& curioLores() const {
        return curioLores_;  // M85
    }
    const std::unordered_map<std::string, CutsceneDef>& cutscenes() const {
        return cutscenes_;  // M97
    }

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
    std::size_t milestoneCount() const { return milestones_.size(); }  // M63
    std::size_t storyCount() const { return story_.size(); }
    std::size_t eventFlavorCount() const { return eventFlavors_.size(); }  // M80
    std::size_t curioLoreCount() const { return curioLores_.size(); }  // M85
    std::size_t cutsceneCount() const { return cutscenes_.size(); }  // M97

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
    std::unordered_map<std::string, MilestoneDef> milestones_;  // M63
    std::vector<StoryBeat> story_;
    std::unordered_map<std::string, EventFlavorDef> eventFlavors_;  // M80
    std::unordered_map<std::string, CurioLoreDef> curioLores_;  // M85
    std::unordered_map<std::string, CutsceneDef> cutscenes_;  // M97
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
