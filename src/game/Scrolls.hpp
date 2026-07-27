#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "game/Character.hpp"

// Scroll learning (M64): using a Scroll-type item teaches ONE character its
// `grantsSkill` permanently (the field existed — validated — since M2, but no
// code ever consumed it until now). Class-agnostic (owner decision); a
// character that already knows the skill refuses, so a scroll is never
// wasted. Pure and raylib-free.

namespace cd {

// Every skill the character commands: the class learnset at its level plus
// its scroll-learned extras, de-duplicated in that order. The single rule
// buildBattle and the party panel both read.
inline std::vector<std::string> allKnownSkills(const Character& c,
                                               const content::ContentDatabase& db) {
    std::vector<std::string> out;
    if (const content::ClassDef* cls = db.findClass(c.classId)) {
        out = content::knownSkillsFor(*cls, c.level);
    }
    for (const std::string& id : c.extraSkills) {
        if (std::find(out.begin(), out.end(), id) == out.end()) {
            out.push_back(id);
        }
    }
    return out;
}

// Empty = the character may learn this scroll; otherwise the reason shown to
// the player.
inline std::string scrollRefusal(const Character& c, const content::ItemDef& item,
                                 const content::ContentDatabase& db) {
    if (item.type != content::ItemType::Scroll || item.grantsSkill.empty()) {
        return "That is not a teaching scroll.";
    }
    if (!db.hasSkill(item.grantsSkill)) {
        return "The scroll's writing is illegible.";  // defensive: bad content
    }
    const std::vector<std::string> known = allKnownSkills(c, db);
    if (std::find(known.begin(), known.end(), item.grantsSkill) != known.end()) {
        return c.name + " already knows that skill.";
    }
    return "";
}

// Records the learned skill (the caller consumes the scroll from the bag and
// shows the message). Call only after scrollRefusal returned empty.
inline void learnScroll(Character& c, const content::ItemDef& item) {
    c.extraSkills.push_back(item.grantsSkill);
}

}  // namespace cd
