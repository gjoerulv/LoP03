#include "states/CelebrationState.hpp"

#include <cmath>
#include <utility>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "states/CelebrationPhrases.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

// Per-party-slot jump character: everyone celebrates to their own beat.
constexpr float kJumpFreq[4] = {2.4f, 3.1f, 2.0f, 2.8f};   // radians/second-ish
constexpr float kJumpAmp[4] = {12.0f, 18.0f, 10.0f, 15.0f};  // pixels
constexpr float kJumpPhase[4] = {0.0f, 1.3f, 2.6f, 0.7f};

constexpr int kGroundY = 196;   // where feet land
constexpr int kSpriteHalf = 24;  // 2x-scaled 24px sprite -> 48px, half = 24

}  // namespace

CelebrationState::CelebrationState(StateStack& stack, AppContext& context, std::string headline,
                                   int mvpIndex)
    : GameState(stack), context_(context), headline_(std::move(headline)), mvp_(mvpIndex) {
    // KO flags snapshotted now: the battle already wrote HP back, so a fallen
    // member celebrates horizontally.
    for (const Character& c : context_.party.members) {
        ko_.push_back(c.hp <= 0);
    }
    if (mvp_ < 0 || mvp_ >= static_cast<int>(context_.party.members.size())) {
        mvp_ = -1;
    }
    // Owner rule (confirmed twice, the second time with feeling): a KO'd MVP
    // KEEPS the pedestal, the chip, and the gold name — they earned the crown
    // and lie in state on it. Too funny not to.
    // One dry line per celebration — the TitlePhrases idiom. GetRandomValue is
    // deterministic under the capture harness's pinned SetRandomSeed, so
    // `84_celebration` stays reproducible; live play draws a fresh one each
    // victory. Presentation-only randomness: never the gameplay Rng.
    punchline_ = kCelebrationPhrases[static_cast<std::size_t>(
        GetRandomValue(0, static_cast<int>(kCelebrationPhrases.size()) - 1))];
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Result);  // the fanfare (no-op if playing)
}

void CelebrationState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Confirm);
        stack().popState();  // on to the reckoning (or the challenge overlay)
    }
}

void CelebrationState::update(float dt) { time_ += dt; }

void CelebrationState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);

    // Deterministic falling confetti (a few colored flecks; pure time function).
    const Color flecks[4] = {p.gold, p.crystal, ui::style::palette().magic, p.success};
    for (int i = 0; i < 28; ++i) {
        const float fx = std::fmod(static_cast<float>(i) * 61.7f + 13.0f, static_cast<float>(w));
        const float speed = 16.0f + static_cast<float>((i % 5)) * 7.0f;
        const float fy =
            std::fmod(static_cast<float>(i) * 37.3f + time_ * speed, static_cast<float>(h));
        DrawRectangle(static_cast<int>(fx), static_cast<int>(fy), 2, 2, flecks[i % 4]);
    }

    ui::drawTitlePlaque("Victory!", w / 2, 18, 20);
    ui::drawTextCentered(headline_.c_str(), w / 2, 64, 20, p.gold);
    // The punchline, dry and small, under the number (width lint-pinned
    // headlessly in test_presentation_options).
    ui::drawTextCentered(punchline_.c_str(), w / 2, 92, 10, p.textDim);

    const auto& members = context_.party.members;
    const int count = static_cast<int>(members.size());

    // Positions: the MVP holds the pedestal at centre; the others fan out
    // around it in party order (nearest slots first). Without an MVP the whole
    // row spreads evenly.
    std::vector<int> centers(static_cast<std::size_t>(count), w / 2);
    if (mvp_ >= 0) {
        const int fan[4] = {w / 2 - 70, w / 2 + 70, w / 2 - 130, w / 2 + 130};
        int slot = 0;
        for (int i = 0; i < count; ++i) {
            if (i == mvp_) {
                centers[static_cast<std::size_t>(i)] = w / 2;
            } else if (slot < 4) {
                centers[static_cast<std::size_t>(i)] = fan[slot++];
            }
        }
    } else {
        for (int i = 0; i < count; ++i) {
            centers[static_cast<std::size_t>(i)] = w / 2 + (i * 2 - (count - 1)) * 45;
        }
    }

    // The pedestal, before its occupant (they stand on it, not behind it).
    const int pedestalTop = kGroundY - 16;
    if (mvp_ >= 0) {
        ui::drawFrame(w / 2 - 26, pedestalTop, 52, 16, ui::FrameStyle::Raised);
        ui::drawChip("MVP", w / 2 - 12, pedestalTop - 66, p.gold);
    }

    for (int i = 0; i < count; ++i) {
        const Character& c = members[static_cast<std::size_t>(i)];
        const int cx = centers[static_cast<std::size_t>(i)];
        const std::string sprId = "actor." + c.classId + ".battle";
        const bool hasTex = context_.resources.hasTexture(sprId);
        const int base = (i == mvp_) ? pedestalTop : kGroundY;

        if (ko_[static_cast<std::size_t>(i)]) {
            // Fallen: horizontal at the ground, dimmed, no jumping.
            if (hasTex) {
                const Texture2D& tex = context_.resources.texture(sprId);
                DrawTexturePro(tex,
                               Rectangle{0, 0, static_cast<float>(tex.width),
                                         static_cast<float>(tex.height)},
                               Rectangle{static_cast<float>(cx),
                                         static_cast<float>(base - kSpriteHalf), 48.0f, 48.0f},
                               Vector2{24.0f, 24.0f}, 90.0f, Color{110, 110, 125, 255});
            }
        } else {
            // Everyone jumps to their own rhythm; the MVP a little higher.
            const std::size_t s = static_cast<std::size_t>(i % 4);
            float amp = kJumpAmp[s];
            if (i == mvp_) {
                amp *= 1.4f;
            }
            const float jump =
                std::fabs(std::sin(time_ * kJumpFreq[s] + kJumpPhase[s])) * amp;
            const int sy = base - 48 - static_cast<int>(jump);
            if (hasTex) {
                DrawTextureEx(context_.resources.texture(sprId),
                              Vector2{static_cast<float>(cx - kSpriteHalf),
                                      static_cast<float>(sy)},
                              0.0f, 2.0f, WHITE);
            }
        }
    }

    // Only the MVP is named (the sprites carry everyone's identity; per-member
    // labels at fan spacing cannot fit the widest names). Centered by measure,
    // shrunk by the fitted helper when even 110px is not enough.
    if (mvp_ >= 0) {
        const std::string& name = members[static_cast<std::size_t>(mvp_)].name;
        const int tw = std::min(110, ui::measureText(name, 8));
        ui::drawTextFitted(name, w / 2 - tw / 2, kGroundY + 6, 110, 8, p.gold,
                           "celebrate.mvpname");
    }

    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Confirm,
                                              context_.input.activeDevice()),
                          "Continue"}},
                        w, h, "celebrate.footer");
}

}  // namespace cd
