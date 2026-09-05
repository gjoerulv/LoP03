#pragma once

#include <memory>
#include <string>
#include <vector>

#include "audio/AudioRoles.hpp"
#include "battle/Battle.hpp"
#include "game/BattleTelemetry.hpp"  // M109: LifetimeHook + the ledger's battle observer
#include "game/RunStats.hpp"
#include "game/Spoils.hpp"
#include "render/BattleBackdrop.hpp"
#include "render/BattleSequencer.hpp"
#include "render/SummonFx.hpp"  // M107
#include "states/AoeTint.hpp"
#include "states/BattleLog.hpp"
#include "states/GameState.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"

namespace cd {

struct AppContext;
struct SpecialEncounter;  // M112: the decision encounters (game/SpecialEncounter.hpp)
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
    // `stage` (M56) selects the per-theme battle backdrop; Plain is the neutral
    // default so every existing call site keeps compiling unchanged.
    // `spoils` (M68), when given, is the defeated team's payout: on Victory the
    // battle itself applies it (gold with standing bonuses, party-wide XP) and
    // the Done beat shows the FF-style results panel — XP, gold, and each
    // member's level-up diff — on the same single Confirm that always ended a
    // battle. Must outlive the state (DungeonState owns it, like resultSlot).
    // M94: `manualEnemies` is the sparring mirror's manual mode — enemy-side
    // units whose turn is not forced/uncontrolled route through the SAME
    // player command phases (Attack/Skill/Guard; never Item or Escape).
    // M109: `lifetime` names the save's ledger this fight records into (and
    // the town it belongs to); the default records nothing, which is what
    // every zero-stakes surface (the spar, capture scenes) passes.
    BattleState(StateStack& stack, AppContext& context, battle::Battle battle,
                battle::BattleResult* resultSlot, MusicTrack musicOverride = MusicTrack::None,
                RunStats* statsSlot = nullptr, bool castleChallenge = false,
                render::BackdropStage stage = render::BackdropStage::Plain,
                const BattleSpoils* spoils = nullptr, bool manualEnemies = false,
                LifetimeHook lifetime = {}, SpecialEncounter* special = nullptr);

    void onEnter() override;  // first-battle tutorial beat
    void handleInput(const Input& input) override;
    void update(float dt) override;
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only (M25): force the target-selection phase (first party member
    // attacking the first living enemy) so the target-info panel renders
    // deterministically for the overflow check. Not present in shipping builds.
    void captureEnterTargeting(int cursor = 0);  // M115: `cursor` = which visual target
    // Capture-only: open the skill list for the acting party member, optionally
    // with a supplied skill set, so the widest name + MP column is overflow-checked.
    void captureEnterSkillMenu(std::vector<std::string> skills = {});
    // Capture-only (M107): freeze a summon's apparition mid-beat so the
    // centered stagecraft renders deterministically for the overflow check.
    void captureShowSummon(const std::string& skillId);
    // Capture-only (M44): open the item list, so the widest item name + count
    // column (the Royal Relics) is overflow-checked.
    void captureEnterItemMenu();
    // Capture-only (M45): show the LONGEST Jester quip, so the mid-screen line is
    // overflow-checked at its worst case.
    void captureShowJest();
    // Capture-only (M48): resolve a real elemental skill against the enemy side
    // and freeze its floats, so the "Weak!" / "Immune" readouts are captured as
    // the game produces them.
    void captureElementHit(const content::SkillDef& skill);
    // Capture-only (M49): fell the King's court and take the turn his revive
    // clock fires on, so the announcement is the shared rule's own words.
    void captureCourtRevival();
    // Capture-only (M51): resolve an all-enemies skill and freeze the impact beat
    // so the AoE screen tint is captured as produced.
    void captureAoeImpact(const std::string& skillId);
    // Capture-only (M68): force the Done phase with a fabricated spoils result
    // (two level-ups incl. new skills), so the victory panel's fullest layout is
    // overflow-checked.
    void captureShowSpoils();
    // Capture-only: open the unit Details overlay on a party actor staged at the
    // fullest layout (guard line + four status chips + a Passive line — the
    // combination that overran the pre-M87 hard budget; it scrolls now).
    void captureOpenDetails();
    // Capture-only (M87): open the skill list and the highlighted skill's full
    // sheet in the scrollable Details overlay.
    void captureOpenSkillDetails(std::vector<std::string> skills = {});
    // M112: resolve the decision encounter as if the first party member had
    // struck placeholder `ordinal` (0..2) with a basic attack, and hold the
    // resulting beat (the Done beat, or the Mimic's revealed impact).
    void captureSpecialPick(int ordinal);
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
    // M112 decision mode: the encounter stands unresolved and every hostile
    // action is a CHOICE, never a hit.
    bool decisionPending() const;
    bool isInert(int index) const;  // a placeholder (answer, Jester, chest)
    void noteRoster();              // bestiary + boss telegraph, skipping inert units
    void pruneOrder();              // inert units never take a turn
    void resolveDecision(int targetUnit);
    void revealMimic(int actor, const content::SkillDef* skill);
    void executeEnemy(int actor);
    void executeConfused(int actor);  // M35: a confused party member auto-attacks an ally
    void executeUncontrolled(int actor);  // M45: the Jester picks its own turn
    void writeBackParty();  // hp/mp write-back, once (level-up heals must survive)
    void maybeApplySpoils();  // M68: on Victory, pay the team's spoils in-battle
    void drawSpoilsPanel() const;  // M68: the Done-beat victory results
    // M42: fold a party action's enemy damage into the run's victory tallies.
    void accumulateStats(const std::vector<int>& hpBefore, int actor, bool offensiveStatus);
    void afterAction();
    void finish();
    std::string outcomeMessage() const;

    // M48: what a float MEANS, so its color is chosen in one place. Damage/Heal/
    // Miss keep exactly the colors they had before; Weak and Immune are the new
    // element readouts (always paired with their own words, never color alone).
    enum class FloatKind { Damage, Heal, Miss, Weak, Immune };

    struct FloatNumber {
        float x = 0.0f;
        float y = 0.0f;
        float timer = 0.0f;
        std::string text;
        FloatKind kind = FloatKind::Damage;
    };
    int enemyBaseY() const;
    // True when any unit on the field is a boss (the M46 violet accent pair).
    bool bossOnField() const;
    // M115: the enemy sprite id drawUnit resolves (boss family, per-enemy
    // art, the class-sprite echo fallback, the tier generic) — shared with
    // the formation so the envelopes come from the very texture drawn.
    std::string enemySpriteId(const battle::Combatant& c, bool& flipX) const;
    // M115: recompute the per-ordinal enemy row lines from every unit's
    // envelope (texture height when loaded, else the family default). Called
    // whenever the roster is set: the ctor and the Mimic morph.
    void rebuildFormation();
    void unitScreenPos(int index, int& outX, int& outY) const;
    // M101: reorder targetCandidates_ to visual top-to-bottom (the center-out
    // rows broke the old unit-order == screen-order equivalence).
    void sortTargetsByScreenY();
    // Computes deltas into the pending presentation (floats, hit flags,
    // SFX); nothing is shown until commitPresentation() runs (at the
    // sequencer's impact beat, or immediately for status ticks).
    // damageSfx picks the impact sound (2 physical, 4 magic); statusAction
    // marks buff/debuff casts that move no HP (SFX code 5).
    void stageNumbers(const std::vector<int>& hpBefore, int damageSfx = 2,
                      bool statusAction = false);
    // M22: pushes the contextual Details overlay for the focused unit.
    void openDetails();
    // M87: full skill/item sheets for the selection phases — the bottom-panel
    // preview stays 2 lines; Details reaches the whole text.
    void openSkillDetails();
    void openItemDetails();
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
    render::BackdropStage stage_ = render::BackdropStage::Plain;  // M56: theme backdrop
    const BattleSpoils* spoils_ = nullptr;  // M68: the team's payout (optional)
    SpoilsResult spoilsResult_;             // M68: what a victory actually paid
    bool spoilsPanel_ = false;              // M68: show the results panel in Done
    bool wroteBack_ = false;                // hp/mp write-back happened
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

    // M51: the flavour of an all-target action being presented, drawn as a faint
    // full-screen tint during the impact beat. Set when such an action resolves,
    // cleared when the presentation sequence ends.
    AoeTint aoeTint_ = AoeTint::None;
    // Capture-only: hold the sequencer at the impact beat (declared always; only
    // set under CRYSTAL_CAPTURE) so the AoE-tint scene renders deterministically.
    bool captureFreezeSeq_ = false;

    std::string message_;
    // M52: presentation-only ring buffer of the exact lines shown after each
    // resolved action, opened as a scrollable overlay by the Menu action. Owned
    // here, so it is freed with the battle; it never touches the battle model.
    BattleLog log_;
    // M49: what a per-turn boss rule announced at the top of this turn (the
    // King's revive clock). Prepended to the acting unit's own message so the
    // court's return is explained in the same beat, then cleared.
    std::string turnOpenLine_;
    // M45: the Jester's current quip, shown mid-screen while it lasts. Purely
    // decorative — nothing reads it back into the battle.
    std::string jestLine_;
    float jestTimer_ = 0.0f;
    // M107: the summon apparition — the creature, large at the battlefield's
    // center, for the resolution beat (timer counts down; duration scales
    // with the message-speed setting like the quip it accompanies).
    render::SummonKind summonFxKind_ = render::SummonKind::Goose;
    float summonFxTimer_ = 0.0f;
    float summonFxDuration_ = 1.0f;
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
    // M91: the resolved action's element — set beside each useSkill/attack
    // call, drawn as a per-element accent on every hit unit during the impact
    // beat (render::drawElementImpact) and steering the impact SFX toward the
    // element's own role in commitPresentation. Presentation-only.
    content::Element fxElement_ = content::Element::None;
    // M94: the sparring mirror's manual mode (see the ctor note).
    bool manualEnemies_ = false;
    // M112: the decision encounter this battle hosts (null for a real fight;
    // owned by the dungeon, outliving the state). `inert_` marks the
    // placeholders — real enemy-side units at 1 HP the model counts as a
    // living side, kept off the turn order, the bestiary and the meters here.
    // A resolved decision ends the battle at the next settled beat with
    // `decisionOutcome_`, or morphs it into the Mimic fight and plays on.
    SpecialEncounter* special_ = nullptr;
    std::vector<char> inert_;
    // M115: the absolute row line y of every enemy ordinal (battle_ui::
    // enemyRowYs), presentation only.
    std::vector<int> enemyRowY_;
    int placeholderFirst_ = -1;
    bool decisionDone_ = false;
    battle::Outcome decisionOutcome_ = battle::Outcome::Ongoing;
    // M109: the ledger this fight records into (stats null = nothing), and the
    // observer that feeds it - owned here, attached to battle_.observer for
    // the life of the state. Declared after battle_ (it refers into it).
    LifetimeHook lifetime_;
    std::unique_ptr<BattleTelemetry> telemetry_;
};

}  // namespace cd
