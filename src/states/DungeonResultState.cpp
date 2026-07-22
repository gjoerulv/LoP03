#include "states/DungeonResultState.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "core/AppContext.hpp"
#include "game/Party.hpp"
#include "core/FadeController.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/DetailsOverlayState.hpp"
#include "states/ScoreDetailsText.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"

namespace cd {

DungeonResultState::DungeonResultState(StateStack& stack, AppContext& context,
                                       score::RunSummary summary, int score,
                                       BossDropResult drops, RunStats stats)
    : GameState(stack),
      context_(context),
      summary_(std::move(summary)),
      score_(score),
      drops_(std::move(drops)),
      stats_(std::move(stats)) {
    context_.fade.start();
    context_.audio.setMusic(MusicTrack::Result);
    context_.audio.setAmbience(AmbienceTrack::None);
}

std::string DungeonResultState::runStatsText() const {
    std::string s = "This run\n";
    s += "Total damage dealt: " + std::to_string(stats_.totalDamage) + "\n";
    s += "Biggest single hit: " + std::to_string(stats_.biggestHit) + "\n";
    s += "Statuses inflicted: " + std::to_string(stats_.statusesInflicted) + "\n";
    const int mvp = stats_.mvpMember();
    if (mvp >= 0 && mvp < static_cast<int>(context_.party.members.size())) {
        s += "MVP: " + context_.party.members[static_cast<std::size_t>(mvp)].name + "\n";
    }
    s += "\nPersonal records\n";
    s += "Biggest hit ever: " + std::to_string(context_.party.recordBiggestHit) + "\n";
    s += "Most damage in a run: " + std::to_string(context_.party.recordRunDamage);
    return s;
}

void DungeonResultState::onEnter() {
    maybeTutorialPrompt(stack(), context_, tutorial::kResultFirst);
}

void DungeonResultState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Details)) {
        stack().pushState(
            std::make_unique<DetailsOverlayState>(stack(), context_, "Run Stats", runStatsText()));
        return;
    }
    if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
        stack().popState();  // close the result
        stack().popState();  // leave the dungeon -> back to town
    }
}

void DungeonResultState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    ClearBackground(Color{14, 18, 22, 255});

    const int boxW = 300;
    // M39: the boss-drop block (a header + up to a token line + a legendary line)
    // grows the panel too, with a small gap separating it from the breakdown.
    const bool anyDrop = drops_.any();
    const int dropLines =
        anyDrop ? (1 + (drops_.tokens > 0 ? 1 : 0) + (drops_.legendary ? 1 : 0)) : 0;
    const int dropGap = anyDrop ? 6 : 0;
    // The panel grows with its line count (escapes and the M20 wager are
    // conditional) so the breakdown never crowds the footer (UI-LAYOUT-018).
    const int lineCount = 6 + (summary_.escapes > 0 ? 1 : 0) +
                          (summary_.wagerAccepted ? 1 : 0) +
                          (summary_.townBonusPct > 0 ? 1 : 0) +        // M32 town bonus
                          (summary_.stakesPenaltyPct > 0 ? 1 : 0) +    // M33 stakes penalty
                          (summary_.classModPct != 0 ? 1 : 0) +        // M45 class modifier
                          dropLines;                                   // M39 boss drops
    // Pitch 13 keeps the common breakdown clear of the footer (audit
    // UI-LAYOUT-018); but the fullest panel (all 10 breakdown lines + the M39
    // drop block) would exceed the virtual height, so when it would, tighten the
    // line pitch just enough to fit inside the screen (footer stays visible).
    const int headerH = 62;
    const int footerH = 34;
    const int maxBoxH = h - 8;  // keep a 4px margin top and bottom
    int pitch = 13;
    int boxH = std::max(190, headerH + lineCount * pitch + dropGap + footerH);
    if (boxH > maxBoxH) {
        pitch = (maxBoxH - headerH - footerH - dropGap) / lineCount;
        boxH = headerH + lineCount * pitch + dropGap + footerH;
    }
    const int boxX = w / 2 - boxW / 2;
    const int boxY = h / 2 - boxH / 2;
    ui::drawFramedPanel(context_.resources, boxX, boxY, boxW, boxH, Color{22, 30, 36, 245}, Color{120, 200, 140, 255});

    ui::drawTextCentered("Dungeon Cleared!", w / 2, boxY + 12, 18, Color{150, 230, 160, 255});
    ui::drawTextCentered(TextFormat("Score: %d", score_), w / 2, boxY + 38, 16, RAYWHITE);

    const score::ScoreBreakdown b = score::scoreBreakdown(summary_);
    int y = boxY + headerH;
    const Color label{200, 200, 215, 255};
    auto line = [&](const char* text, int value, Color color) {
        ui::drawText(text, boxX + 22, y, 10, label);
        ui::drawText(TextFormat("%+d", value), boxX + boxW - 70, y, 10, color);
        y += pitch;
    };
    const Color plus{150, 220, 150, 255};
    const Color minus{225, 150, 150, 255};
    line("Base + boss", b.base + b.bossBonus, plus);
    line(TextFormat("Battle turns (%d)", summary_.battleTurns), -b.turnPenalty, minus);
    line(TextFormat("Chests (%d)", summary_.chestsOpened), b.chestBonus, plus);
    line(TextFormat("Danger defeated (%d)", summary_.dangerDefeated), b.dangerBonus, plus);
    line("Treasure", b.treasureBonus, plus);
    line(summary_.noDeath ? "No-death bonus" : "No-death bonus (lost)", b.noDeathBonus, plus);
    if (summary_.escapes > 0) {
        line(TextFormat("Escapes (%d)", summary_.escapes), -b.escapePenalty, minus);
    }
    if (summary_.wagerAccepted) {
        line(b.wager >= 0 ? "Omen wager won" : "Omen wager lost", b.wager,
             b.wager >= 0 ? plus : minus);
    }
    if (summary_.townBonusPct > 0) {
        line(TextFormat("Town bonus (+%d%%)", summary_.townBonusPct), b.townBonus, plus);
    }
    if (summary_.stakesPenaltyPct > 0) {
        line(TextFormat("Stakes penalty (-%d%%)", summary_.stakesPenaltyPct), -b.stakesPenalty,
             minus);
    }

    if (summary_.classModPct != 0) {
        // M45: shown honestly, signed, with the percentage that produced it.
        line(TextFormat("Class modifier (%+d%%)", summary_.classModPct), b.classMod,
             b.classMod >= 0 ? plus : minus);
    }

    // M39: boss drops, as a reward block below the score breakdown. The legendary
    // line is full-width (the item name is longer than the value column).
    if (anyDrop) {
        y += dropGap;
        const Color gold{235, 210, 130, 255};
        ui::drawText("Boss drops", boxX + 22, y, 10, Color{230, 220, 140, 255});
        y += pitch;
        if (drops_.tokens > 0) {
            ui::drawText("Legendary tokens", boxX + 22, y, 10, label);
            ui::drawText(TextFormat("+%d", drops_.tokens), boxX + boxW - 70, y, 10, gold);
            y += pitch;
        }
        if (drops_.legendary) {
            const content::ItemDef* it = context_.content.findItem(drops_.legendaryId);
            const std::string name = it != nullptr ? it->name : drops_.legendaryId;
            ui::drawText(("Won  " + name).c_str(), boxX + 22, y, 10, gold);
            y += pitch;
        }
    }

    ui::drawTextCentered((input::prompt(context_.input.map(), InputAction::Confirm,
                                        context_.input.activeDevice(), "Return to Town") +
                          "   " +
                          input::prompt(context_.input.map(), InputAction::Details,
                                        context_.input.activeDevice(), "Run stats"))
                             .c_str(),
                         w / 2, boxY + boxH - 16, 10, Color{200, 200, 160, 255});
}

}  // namespace cd
