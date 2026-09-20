#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

// M64: the detailed party panel, reachable from BOTH pause menus. A
// bestiary-style member list + detail: derived stats with the gear share,
// equipment names, passives, level/XP, milestone choices (M63), and every
// known skill (learnset + scroll extras).
//
// M122 (owner request): Confirm on a member opens that member's SKILL LIST -
// kind icons, MP costs, greyed rows the cursor can rest on, Details for the
// full sheet (M121) - and, inside a dungeon only, a healing skill can be CAST
// from here for its normal MP cost (game/FieldSkills.hpp holds the rules; the
// amount is the battle's own). Scrolls are no longer taught here: that moved
// to the Items screen (InventoryState), where the scroll is picked first.

namespace cd {

struct AppContext;
class StateStack;

namespace content {
struct SkillDef;
}

class PartyState : public GameState {
public:
    // `inDungeon`: true when opened from the dungeon pause menu - the only
    // place a heal may be cast from this panel (a town has its Inn).
    PartyState(StateStack& stack, AppContext& context, bool inDungeon = false);

    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    void captureSelect(int member) { cursor_ = member; }
    // Capture-only (M87): open the full member sheet (the Details overlay).
    void captureOpenDetails() { openMemberDetails(); }
    // Capture-only (M122): open the shown member's skill list on `skillRow`;
    // `pickTarget` also enters the target pick when that skill allows it.
    void captureOpenSkills(int skillRow, bool pickTarget);
#endif

private:
    enum class Phase { Browse, Skills, PickTarget };

    void rebuildSkills();  // -> skillMenu_ from the shown member's known skills
    // M87: pushes the scrollable Details overlay with the selected member's
    // full sheet — complete passive/milestone texts and every known skill
    // WITH its description (the compact panel only ever previews these).
    void openMemberDetails();
    void openSkillDetails();  // M122: the M121 skill sheet, as this member casts it
    void confirmSkill();      // M122: cast, open the target pick, or say why not
    void confirmTarget();     // M122: the single-target cast on the picked member
    void afterCast(std::string line);
    const content::SkillDef* currentSkill() const;
    void renderSkillPanel(int dx, int dy, int dw, int dh) const;

    AppContext& context_;
    bool inDungeon_ = false;
    Phase phase_ = Phase::Browse;
    int cursor_ = 0;        // member index: the shown member, and the caster
    int targetCursor_ = 0;  // PickTarget: the member being aimed at
    ui::Menu skillMenu_;
    ui::ScrollWindow skillScroll_;
    std::vector<std::string> skillIds_;  // parallel to skillMenu_ rows
    std::string message_;
    bool messageIsError_ = false;  // M67: banner kind for the feedback toast
};

}  // namespace cd
