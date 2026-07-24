#include "states/BossIntroState.hpp"

#include <cmath>
#include <memory>

#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "settings/Settings.hpp"
#include "states/BattleState.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kShardCount = 14;  // 12-16 per the plan

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
}  // namespace

BossIntroState::BossIntroState(StateStack& stack, AppContext& context, battle::Battle battle,
                               battle::BattleResult* resultSlot, MusicTrack music,
                               RunStats* statsSlot, bool castleChallenge,
                               render::BackdropStage stage, std::uint64_t introSeed)
    : GameState(stack),
      context_(context),
      battle_(std::move(battle)),
      resultSlot_(resultSlot),
      music_(music),
      statsSlot_(statsSlot),
      castleChallenge_(castleChallenge),
      stage_(stage),
      introSeed_(introSeed),
      shards_(buildIntroShards(introSeed, kShardCount)) {
    // The boss name + telegraph shown during the intro (copied out now, so
    // render never reaches back into the battle after it is moved into
    // BattleState).
    for (const battle::Combatant& c : battle_.units) {
        if (c.side == battle::Side::Enemy && c.isBoss) {
            bossName_ = c.name;
            if (!c.telegraph.empty()) {
                telegraph_ = c.telegraph;
            }
            break;
        }
    }
    if (bossName_.empty()) {
        bossName_ = "A boss appears!";
    }
}

#ifdef CRYSTAL_CAPTURE
void BossIntroState::captureSetTime(float t) {
    timeline_.t = t;
    captureFrozen_ = true;  // hold this frame through the harness's settle-updates
}
#endif

void BossIntroState::launchBattle() {
    launched_ = true;
    // The battle reveals from black (BattleState::onEnter does not fade on its own).
    context_.fade.start();
    stack().pushState(std::make_unique<BattleState>(stack(), context_, std::move(battle_),
                                                    resultSlot_, music_, statsSlot_,
                                                    castleChallenge_, stage_));
}

void BossIntroState::update(float dt) {
    if (launched_ || captureFrozen_) {
        return;  // waiting for the battle to pop, or frozen for a capture
    }
    timeline_.advance(dt);
    if (timeline_.done()) {
        launchBattle();
    }
}

void BossIntroState::handleInput(const Input& input) {
    if (launched_) {
        return;
    }
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        timeline_.skip();  // jump to Handoff; the battle still lands from there
    }
}

void BossIntroState::onResume() {
    if (launched_) {
        stack().popState();  // the battle popped -> remove the intro too
    }
}

void BossIntroState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    const float cx = static_cast<float>(w) * 0.5f;
    const float cy = static_cast<float>(h) * 0.5f;
    const settings::Settings& sv = context_.settings.values;

    // Shake offset during Peak, honouring the shake setting (deterministic jitter
    // from the timeline clock so capture is stable).
    float sx = 0.0f;
    float sy = 0.0f;
    if (sv.effectShake != settings::EffectLevel::Off) {
        const float pulse = timeline_.peakPulse();
        if (pulse > 0.0f) {
            const float amp =
                (sv.effectShake == settings::EffectLevel::Reduced ? 1.5f : 3.0f) * pulse;
            const int f = static_cast<int>(timeline_.t * 120.0f);
            sx = static_cast<float>((f % 3) - 1) * amp;
            sy = static_cast<float>(((f / 3) % 3) - 1) * amp;
        }
    }

    // Uncapped darkening fade over the frozen scene (not a flash).
    const float dark = timeline_.darkness();
    if (dark > 0.0f) {
        DrawRectangle(0, 0, w, h,
                      Color{0, 0, 0, static_cast<unsigned char>(clamp01(dark) * 255.0f)});
    }

    const float crack = timeline_.crackProgress();
    const float ho = timeline_.handoffProgress();

    // Cracks: radial ink-outlined crystal lines from centre, growing with the
    // build. Drawn during Build/Peak (before the shards take over).
    if (crack > 0.0f && ho <= 0.0f) {
        const float maxLen = 150.0f;
        for (int i = 0; i < 10; ++i) {
            const float ang = (static_cast<float>(i) / 10.0f) * 6.2831853f +
                              (i % 2 == 0 ? 0.0f : 0.31f);
            const float len = crack * maxLen * (0.7f + 0.06f * static_cast<float>(i % 5));
            const Vector2 a{cx + sx, cy + sy};
            const Vector2 b{cx + sx + std::cos(ang) * len, cy + sy + std::sin(ang) * len};
            DrawLineEx(a, b, 3.0f, p.ink);       // ink outline
            DrawLineEx(a, b, 1.0f, p.crystal);   // crystal core
        }
    }

    // Boss name + telegraph, fading in with the build, over the darkening.
    if (crack > 0.0f && ho <= 0.0f && !bossName_.empty()) {
        const unsigned char alpha = static_cast<unsigned char>(clamp01(crack * 1.6f) * 255.0f);
        Color nameCol = p.gold;
        nameCol.a = alpha;
        ui::drawTextCentered(bossName_.c_str(), static_cast<int>(cx + sx),
                             static_cast<int>(cy - 22.0f + sy), 20, nameCol);
        if (!telegraph_.empty()) {
            Color tCol = p.text;
            tCol.a = alpha;
            ui::drawTextWrappedCentered(telegraph_, static_cast<int>(cx + sx),
                                        static_cast<int>(cy + 4.0f + sy), w - 60, style::kFontBody,
                                        tCol, "bossintro.telegraph", 3);
        }
    }

    // Peak pulse: a single dim overlay, alpha capped at 0.12 and gated by the
    // flash setting (the M51 AoE-tint contract; never a strobe).
    const float a = bossIntroPulseAlpha(timeline_.peakPulse(), sv.effectFlash);
    if (a > 0.0f) {
        DrawRectangle(0, 0, w, h, Color{0, 0, 0, static_cast<unsigned char>(a * 255.0f)});
    }

    // Handoff: crystal shards flung outward over the (now black) field, fading as
    // they fly. Seeded layout — no per-frame randomness, no battle RNG.
    if (ho > 0.0f) {
        const float maxTravel = 0.62f * static_cast<float>(w);
        const unsigned char shardA = static_cast<unsigned char>(clamp01(1.0f - ho) * 255.0f);
        Color core = p.crystal;
        core.a = shardA;
        Color ink = p.ink;
        ink.a = shardA;
        for (const IntroShard& s : shards_) {
            const float d = ho * s.speed * maxTravel;
            const int x = static_cast<int>(cx + s.dirX * d + sx);
            const int y = static_cast<int>(cy + s.dirY * d + sy);
            DrawRectangle(x - 1, y - 1, s.size + 2, s.size + 2, ink);
            DrawRectangle(x, y, s.size, s.size, core);
        }
    }
}

}  // namespace cd
