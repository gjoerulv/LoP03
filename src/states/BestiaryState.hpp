#pragma once

#include <string>
#include <vector>

#include "content/Stats.hpp"
#include "states/GameState.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;

// The bestiary (M42): a codex of every foe in the game. Entries the party has
// fought (Party.encountered) are revealed with sprite, stats, behaviour profile,
// passives, and flavor; the rest are listed as unknowns so the roster's size --
// and the player's progress through it -- is always visible. Presentation only.
class BestiaryState : public GameState {
public:
    BestiaryState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only: park the cursor on a given foe so its detail panel renders
    // deterministically for the overflow check. Not present in shipping builds.
    void captureSelect(const std::string& id);
#endif

private:
    struct Entry {
        std::string id;
        std::string spriteId;   // enemy.<id>.battle / boss.<id>.battle
        std::string name;
        content::StatBlock stats;
        std::string profile;                 // tier + role + tags, or boss archetype
        std::vector<std::string> passives;   // revealed passive names, one per line
        // M48: the foe's element affinities, pre-formatted for display ("Fire",
        // "Holy"). Empty for an untagged foe, so its panel is unchanged.
        std::vector<std::string> weakTo;
        std::vector<std::string> immuneTo;
        std::string flavor;                  // boss description, or empty
        bool boss = false;
        bool known = false;                  // party has faced it
        // M52: the strongest real fight context this foe appears in, as a
        // stat-scale percent (foeMaxScalePct), for the "at their strongest"
        // readout. Computed once at construction where the content DB is handy.
        int maxScalePct = 0;
    };

    void renderDetail(const Entry& entry, int px, int pw);

    AppContext& context_;
    std::vector<Entry> entries_;
    ui::ScrollWindow scroll_;
    int cursor_ = 0;
    int known_ = 0;
};

}  // namespace cd
