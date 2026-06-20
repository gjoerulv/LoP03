#pragma once

#include <string>
#include <vector>

#include "battle/Battle.hpp"
#include "states/GameState.hpp"
#include "ui/Menu.hpp"

namespace cd {

struct AppContext;

// Side-view battle screen (enemies left, party right) driving the pure Battle
// model. Speed-ordered turns; Attack/Skill/Item/Guard/Escape; deterministic
// resolution. On completion it writes hp/mp back to the party, stores the
// outcome in the provided slot, and pops.
class BattleState : public GameState {
public:
    BattleState(StateStack& stack, AppContext& context, battle::Battle battle,
                battle::BattleResult* resultSlot);

    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

private:
    enum class Phase { Intro, Command, ChooseTarget, ChooseSkill, ChooseItem, Resolve, Done };
    enum class PendingKind { Attack, Skill, Item };

    int currentActor() const;
    void beginTurns();
    void startActorTurn();
    void advanceTurn();
    void buildCommandMenu();
    void buildSkillMenu();
    void buildItemMenu();
    std::vector<std::string> consumableIds() const;

    void onCommand();
    void onSkillChosen();
    void onItemChosen();
    void executePending(int targetUnit);
    void executeEnemy(int actor);
    void afterAction();
    void finish();
    std::string outcomeMessage() const;

    void drawUnit(const battle::Combatant& c, int x, int y, bool current, bool targeted) const;

    AppContext& context_;
    battle::Battle battle_;
    battle::BattleResult* resultSlot_;
    battle::Outcome result_ = battle::Outcome::Ongoing;
    bool bossBattle_ = false;
    bool koOccurred_ = false;

    Phase phase_ = Phase::Intro;
    std::vector<int> order_;
    int orderPos_ = 0;

    ui::Menu commandMenu_;
    ui::Menu skillMenu_;
    ui::Menu itemMenu_;
    std::vector<std::string> skillIds_;
    std::vector<std::string> itemIds_;

    PendingKind pendingKind_ = PendingKind::Attack;
    std::string pendingSkillId_;
    std::string pendingItemId_;
    std::vector<int> targetCandidates_;
    int targetCursor_ = 0;

    std::string message_;
    float timer_ = 0.0f;
};

}  // namespace cd
