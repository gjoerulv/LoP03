#pragma once

#include <string>
#include <vector>

#include "audio/AudioRoles.hpp"
#include "battle/Battle.hpp"
#include "game/RunStats.hpp"
#include "render/BattleSequencer.hpp"
#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;
namespace content {
struct ItemDef;
}

// Side-view battle screen (enemies left, party right) driving the pure Battle
// model. Speed-ordered turns; Attack/Skill/Item/Guard/Escape; deterministic
// resolution. On completion it writes hp/mp back to the party, stores the
// outcome in the provided slot, and pops.
class BattleState : public GameState {
public:
    // `musicOverride` (M40) replaces the default Boss/Battle track for this fight
    // (e.g. the King's own theme); None keeps the default. `statsSlot` (M42), when
    // given, accumulates this battle's victory tallies into a run's RunStats.
    // `castleChallenge` (M43) marks a fight fought at the castle, where losing
    // costs no gold and forfeits no run — so the defeat message tells the truth.
    BattleState(StateStack& stack, AppContext& context, battle::Battle battle,
                battle::BattleResult* resultSlot, MusicTrack musicOverride = MusicTrack::None,
                RunStats* statsSlot = nullptr, bool castleChallenge = false);

    void onEnter() override;  // first-battle tutorial beat
    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M25): force the target-selection phase (first party member
    // attacking the first living enemy) so the target-info panel renders
    // deterministically for the overflow check. Not present in shipping builds.
    void captureEnterTargeting();
    // Capture-only: open the skill list for the acting party member, optionally
    // with a supplied skill set, so the widest name + MP column is overflow-checked.
    void captureEnterSkillMenu(std::vector<std::string> skills = {});
    // Capture-only (M44): open the item list, so the widest item name + count
    // column (the Royal Relics) is overflow-checked.
    void captureEnterItemMenu();
#endif

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
    // M43: why a battle item is unusable right now ("" when it is usable).
    std::string itemBlockReason(const content::ItemDef& item) const;
    void executePending(int targetUnit);
    void executeEnemy(int actor);
    void executeConfused(int actor);  // M35: a confused party member auto-attacks an ally
    // M42: fold a party action's enemy damage into the run's victory tallies.
    void accumulateStats(const std::vector<int>& hpBefore, int actor, bool offensiveStatus);
    void afterAction();
    void finish();
    std::string outcomeMessage() const;

    struct FloatNumber {
        float x = 0.0f;
        float y = 0.0f;
        float timer = 0.0f;
        std::string text;
        bool heal = false;
    };
    int enemyBaseY() const;
    void unitScreenPos(int index, int& outX, int& outY) const;
    // Computes deltas into the pending presentation (floats, hit flags,
    // SFX); nothing is shown until commitPresentation() runs (at the
    // sequencer's impact beat, or immediately for status ticks).
    // damageSfx picks the impact sound (2 physical, 4 magic); statusAction
    // marks buff/debuff casts that move no HP (SFX code 5).
    void stageNumbers(const std::vector<int>& hpBefore, int damageSfx = 2,
                      bool statusAction = false);
    // M22: pushes the contextual Details overlay for the focused unit.
    void openDetails();
    void commitPresentation();

    void drawUnit(const battle::Combatant& c, int index, int x, int y, bool current,
                  bool targeted) const;

    AppContext& context_;
    battle::Battle battle_;
    battle::BattleResult* resultSlot_;
    MusicTrack musicOverride_ = MusicTrack::None;
    RunStats* stats_ = nullptr;  // M42: run victory-stat accumulation (optional)
    battle::Outcome result_ = battle::Outcome::Ongoing;
    bool castleChallenge_ = false;  // M43: castle defeats cost no gold
    bool bossBattle_ = false;
    bool koOccurred_ = false;
    std::string bossTelegraph_;

    Phase phase_ = Phase::Intro;
    std::vector<int> order_;
    int orderPos_ = 0;

    ui::Menu commandMenu_;
    ui::Menu skillMenu_;
    ui::Menu itemMenu_;
    ui::ScrollWindow skillScroll_;
    ui::ScrollWindow itemScroll_;
    std::vector<std::string> skillIds_;
    std::vector<std::string> itemIds_;

    PendingKind pendingKind_ = PendingKind::Attack;
    std::string pendingSkillId_;
    std::string pendingItemId_;
    std::vector<int> targetCandidates_;
    int targetCursor_ = 0;

    std::string message_;
    std::vector<FloatNumber> floats_;

    // Staged presentation (M18): the sim result is already final; these only
    // control when it becomes visible.
    render::BattleSequencer seq_;
    std::vector<int> displayHp_;            // HP as currently shown (commits at impact)
    std::vector<FloatNumber> pendingFloats_;
    std::vector<char> hitFlags_;            // units brightened during the impact beat
    std::vector<float> koFade_;             // enemy fade-out after a shown KO (1 -> 0)
    int lungeUnit_ = -1;                    // acting unit during the current sequence
    int pendingSfx_ = 0;                    // 0 none, 1 heal, 2 hit, 3 ko
};

}  // namespace cd
