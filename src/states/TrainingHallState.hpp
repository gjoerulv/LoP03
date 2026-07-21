#pragma once

#include <string>
#include <vector>

#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// The Training Hall: pay gold to level a party member up by one (stats recompute
// from the class growth, and the HP gained is restored), and (M36) buy passive
// skills per character — own many, equip one, swap freely.
class TrainingHallState : public GameState {
public:
    TrainingHallState(StateStack& stack, AppContext& context);

    void onEnter() override;
    void handleInput(const Input& input) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Deterministic entry into the passive-management screen for capture scenes.
    void captureEnterPassives();
#endif

private:
    enum class Phase { Members, CharMenu, Passives };

    void rebuildMembers();
    void rebuildCharMenu();
    void rebuildPassives();
    int trainingCost(int level) const;
    void trainSelected();
    void onPassiveConfirm();

    AppContext& context_;
    Phase phase_ = Phase::Members;
    ui::Menu memberMenu_;
    ui::Menu charMenu_;
    ui::Menu passiveMenu_;
    int selectedMember_ = 0;
    std::vector<std::string> passiveIds_;  // sorted ids backing the passive menu
    std::string message_;
};

}  // namespace cd
