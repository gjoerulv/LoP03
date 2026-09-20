#include "states/FallenState.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "core/FadeController.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"
#include "states/EndgameSummaryState.hpp"
#include "states/FallenPhrases.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

constexpr int kGroundY = 204;  // where the fallen lie and the joggers run

// The tormentors, one per party slot: they hop on the member below them, each
// to its own beat (the celebration's jump table, turned on the party).
constexpr const char* kHoppers[4] = {
    "enemy.evil_goose_vanguard.battle",
    "enemy.pond_drake.battle",
    "enemy.evil_goose_trickster.battle",
    "enemy.mallard_marauder.battle",
};
constexpr float kHopFreq[4] = {3.1f, 2.4f, 2.8f, 3.5f};
constexpr float kHopAmp[4] = {22.0f, 30.0f, 18.0f, 26.0f};
constexpr float kHopPhase[4] = {0.0f, 1.1f, 2.3f, 0.6f};
// Party slots fan out either side of the King, whose column stays clear.
constexpr int kFan[4] = {-150, -62, 62, 150};

// Foreground joggers: a lap of honour, left to right and back.
struct Jogger {
    const char* sprite;
    float speed;   // px / second (negative = right to left)
    float offset;  // px along the lap at t = 0
    int yLift;     // px above the ground line
};
constexpr Jogger kJoggers[3] = {
    {"enemy.evil_goose_hexwing.battle", 46.0f, 0.0f, 0},
    {"enemy.gander_grenadier.battle", -38.0f, 210.0f, 4},
    {"enemy.golden_goose.battle", 64.0f, 330.0f, 2},
};

void drawSprite(ResourceManager& resources, const char* id, float x, float y, float scale,
                bool flip, Color tint) {
    if (!resources.hasTexture(id)) {
        return;
    }
    const Texture2D& tex = resources.texture(id);
    const float tw = static_cast<float>(tex.width);
    const float th = static_cast<float>(tex.height);
    DrawTexturePro(tex, Rectangle{0, 0, flip ? -tw : tw, th},
                   Rectangle{std::floor(x), std::floor(y), tw * scale, th * scale},
                   Vector2{0.0f, 0.0f}, 0.0f, tint);
}

}  // namespace

FallenState::FallenState(StateStack& stack, AppContext& context, ironman::FallenInfo info)
    : GameState(stack), context_(context), info_(std::move(info)) {
    // One dry line per fall - the CelebrationState idiom: GetRandomValue is
    // deterministic under the capture harness's pinned seed; live play draws
    // a fresh one. Presentation-only randomness, never the gameplay Rng.
    punchline_ = kFallenPhrases[static_cast<std::size_t>(
        GetRandomValue(0, static_cast<int>(kFallenPhrases.size()) - 1))];
}

void FallenState::onEnter() {
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Defeat);
    context_.audio.setAmbience(AmbienceTrack::None);
}

void FallenState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Confirm);
        // On to the run's summary - a copy of the party as it fell, so the
        // pages stay true whatever the title screen does next.
        stack().clearStates();
        stack().pushState(std::make_unique<EndgameSummaryState>(
            stack(), context_, context_.party, info_, /*leaveToTitle=*/true));
    }
}

void FallenState::update(float dt) {
    if (!frozen_) {
        time_ += dt;
    }
}

void FallenState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);
    ResourceManager& res = context_.resources;

    // Drifting feathers (the celebration's confetti, plucked): pure functions
    // of the clock, swaying as they fall.
    for (int i = 0; i < 22; ++i) {
        const float fi = static_cast<float>(i);
        const float speed = 9.0f + static_cast<float>(i % 5) * 4.0f;
        const float fy = std::fmod(fi * 41.3f + time_ * speed, static_cast<float>(h));
        const float fx = std::fmod(fi * 57.9f + 9.0f, static_cast<float>(w)) +
                         std::sin(time_ * 1.3f + fi) * 6.0f;
        DrawRectangle(static_cast<int>(fx), static_cast<int>(fy), 3, 1,
                      i % 3 == 0 ? p.textDim : p.text);
    }

    ui::drawTitlePlaque("Defeat!", w / 2, 14, 20);
    // Where and to whom (fitted: the longest lines the game can produce are
    // pinned by the capture specimen), then the dry line.
    const std::string fell = "Fell at " + info_.place;
    const std::string beaten = "Beaten by " + info_.foes;
    const int lineW = w - 32;
    const int fellW = std::min(lineW, ui::measureText(fell, ui::style::kFontBody));
    ui::drawTextFitted(fell, w / 2 - fellW / 2, 44, lineW, ui::style::kFontBody, p.text,
                       "fallen.place");
    const int beatenW = std::min(lineW, ui::measureText(beaten, ui::style::kFontBody));
    ui::drawTextFitted(beaten, w / 2 - beatenW / 2, 56, lineW, ui::style::kFontBody,
                       p.dangerText, "fallen.foes");
    ui::drawTextCentered(punchline_.c_str(), w / 2, 70, ui::style::kFontBody, p.textDim);

    // The Hollow King, in the back, laughing: a quick shoulder-shake and a
    // "Ha!" that pops up on alternating sides.
    const float shake = std::sin(time_ * 16.0f) * 1.5f;
    const float kingX = static_cast<float>(w) / 2.0f - 36.0f;
    const float kingY = 98.0f + shake;
    drawSprite(res, "boss.the_hollow_king.battle", kingX, kingY, 2.0f, false,
               Color{190, 190, 205, 255});
    const int beat = static_cast<int>(time_ * 2.5f);
    const float rise = std::fmod(time_ * 2.5f, 1.0f) * 8.0f;
    const char* laugh = beat % 3 == 2 ? "Ha ha!" : "Ha!";
    const int laughW = ui::measureText(laugh, ui::style::kFontBody);
    ui::drawText(laugh, w / 2 - laughW / 2 + (beat % 2 == 0 ? -12 : 12),
                 94 - static_cast<int>(rise), ui::style::kFontBody, p.gold);

    // The party, where it fell - and its tormentors, landing on it in turn.
    const auto& members = context_.party.members;
    const int count = static_cast<int>(members.size());
    for (int i = 0; i < count; ++i) {
        const Character& c = members[static_cast<std::size_t>(i)];
        const std::size_t s = static_cast<std::size_t>(i % 4);
        const int cx = w / 2 + kFan[s];
        const float hop = std::fabs(std::sin(time_ * kHopFreq[s] + kHopPhase[s])) * kHopAmp[s];
        const bool impact = hop < 4.0f;

        const std::string sprId = "actor." + c.classId + ".battle";
        if (res.hasTexture(sprId)) {
            const Texture2D& tex = res.texture(sprId);
            // Lying down (the celebration's KO pose), squashed a pixel or two
            // and flashed pale on every landing.
            DrawTexturePro(tex,
                           Rectangle{0, 0, static_cast<float>(tex.width),
                                     static_cast<float>(tex.height)},
                           Rectangle{static_cast<float>(cx),
                                     static_cast<float>(kGroundY - 22 + (impact ? 2 : 0)), 48.0f,
                                     48.0f},
                           Vector2{24.0f, 24.0f}, 90.0f,
                           impact ? Color{235, 200, 200, 255} : Color{170, 170, 185, 255});
        }
        if (impact) {  // the landing: two small stars either side
            DrawRectangle(cx - 27, kGroundY - 34, 2, 2, p.gold);
            DrawRectangle(cx + 25, kGroundY - 38, 2, 2, p.gold);
        }
        // The hopper faces right as drawn; every other one is turned around.
        drawSprite(res, kHoppers[s], static_cast<float>(cx - 24),
                   static_cast<float>(kGroundY - 40 - 48) - hop, 2.0f, i % 2 == 1, WHITE);
    }

    // The lap of honour, in front of everything.
    const float lap = static_cast<float>(w) + 96.0f;
    for (const Jogger& j : kJoggers) {
        float x = std::fmod(j.offset + time_ * std::fabs(j.speed), lap) - 48.0f;
        const bool rightToLeft = j.speed < 0.0f;
        if (rightToLeft) {
            x = static_cast<float>(w) - x - 48.0f;
        }
        const float bob = std::fabs(std::sin(time_ * 9.0f + j.offset)) * 3.0f;
        drawSprite(res, j.sprite, x, static_cast<float>(kGroundY - 30 - j.yLift) - bob, 2.0f,
                   rightToLeft, WHITE);
    }

    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Confirm,
                                              context_.input.activeDevice()),
                          "Continue"}},
                        w, h, "fallen.footer");
}

}  // namespace cd
