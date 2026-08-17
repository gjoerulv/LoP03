#include "states/GuildState.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "core/SeedParse.hpp"
#include "dungeon/DungeonGenerator.hpp"
#include "game/Party.hpp"
#include "game/StakesLadder.hpp"  // M105: the Eternal entry raises the baseline
#include "game/WorldLadder.hpp"   // kTownCount
#include "states/ConfirmPromptState.hpp"  // M105: the Eternal warning
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "save/SaveSystem.hpp"
#include "states/CastleChallengeState.hpp"
#include "states/DungeonState.hpp"
#include "states/StateStack.hpp"
#include "states/TutorialPromptState.hpp"
#include "tutorial/Tutorial.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kEnter = 0;
constexpr int kTheme = 1;
constexpr int kDepth = 2;
constexpr int kFloors = 3;    // M82: 1-or-4-floor runs
constexpr int kGuildBoss = 4;  // M84: the town's Guild Master gauntlet
// M88: "New Seed" and the seed readout merged into one Seed row — the value
// sits in a capsule on the row that changes it (the M25 stepper idiom):
// Left/Right rerolls, Confirm opens the manual digit editor.
constexpr int kSeed = 5;
constexpr int kBack = 6;
constexpr int kMaxDepth = 20;
// M105: the Floors stepper's fourth value — town 7's Eternal endless descent
// (a sentinel outside the real shapes 1/4/20).
constexpr int kEternalFloors = 0;

std::uint64_t randomSeed() {
    const std::uint64_t hi = static_cast<std::uint64_t>(GetRandomValue(1, 2000000000));
    const std::uint64_t lo = static_cast<std::uint64_t>(GetRandomValue(1, 2000000000));
    return (hi << 21) ^ lo ^ 0xD1B54A32D192ED03ull;
}
}  // namespace

GuildState::GuildState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    rebuild();
}

void GuildState::rebuild() {
    // The menu model still drives navigation; Theme/Depth values render in
    // framed capsules on their own rows (M46 stepper rows), so labels stay
    // plain. The value still sits on the row that changes it (M25 slice 4).
    const int previous = menu_.cursor();
    std::vector<ui::MenuItem> items;
    items.push_back({"Enter Dungeon", true});
    items.push_back({"Theme", true});
    items.push_back({"Depth", true});
    items.push_back({"Floors", true});  // M82
    // M84: kept enabled even while locked so the row stays reachable — the
    // banner under the panel explains the lock, and Confirm refuses politely
    // (ui::Menu skips disabled rows entirely, which would hide the goal).
    items.push_back({"Fight the Guild Boss", true});
    items.push_back({"Seed", true});  // M88: reroll with Left/Right, type with Confirm
    items.push_back({"Back", true});
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
}

void GuildState::onEnter() {
    context_.audio.setMusic(MusicTrack::Guild);  // preparation scene (M21)
    maybeTutorialPrompt(stack(), context_, tutorial::kGuildPrepare);
    themeIds_.clear();
    for (const auto& [id, def] : context_.content.themes()) {
        // M106: a theme is offered only from its minTown on (the Goosy
        // Gauntlet belongs to town 7 alone; every classic theme is 1).
        if (def.minTown <= context_.party.currentTown) {
            themeIds_.push_back(id);
        }
    }
    std::sort(themeIds_.begin(), themeIds_.end());
    if (themeIndex_ >= static_cast<int>(themeIds_.size())) {
        themeIndex_ = 0;
    }
    seed_ = randomSeed();
    rebuild();  // themeIds_ now populated: refresh the inline Theme label
}

void GuildState::onResume() {
    // Back from the dungeon (or a submenu): restore the preparation scene.
    context_.audio.setMusic(MusicTrack::Guild);
    context_.audio.setAmbience(AmbienceTrack::None);
}

#ifdef CRYSTAL_CAPTURE
void GuildState::captureFocusGuildBoss() { menu_.setCursor(kGuildBoss); }

void GuildState::captureOpenSeedEditor() {
    menu_.setCursor(kSeed);
    seedInput_.setValue("18446744073709551615");  // the 20-digit uint64 ceiling
    seedEditing_ = true;
}
#endif

std::string GuildState::currentThemeName() const {
    if (themeIds_.empty()) {
        return "Dungeon";
    }
    const std::string& id = themeIds_[static_cast<std::size_t>(themeIndex_)];
    if (const content::DungeonThemeDef* theme = context_.content.findTheme(id)) {
        return theme->name;
    }
    return id;
}

void GuildState::enterDungeon() {
    const std::string eternalTheme =
        themeIds_.empty() ? "" : themeIds_[static_cast<std::size_t>(themeIndex_)];
    if (floors_ == kEternalFloors) {
        // M105: the warning states the WHOLE price before anything commits;
        // the descent itself then raises the stakes baseline to (town 7,
        // depth 20) — immediately, whatever happens below (owner decision) —
        // and the autosave carries it, so no reload can shed it.
        stack().pushState(std::make_unique<ConfirmPromptState>(
            stack(), context_, "The Eternal Descent",
            "No score - only the record of how deep you stood. Endless floors, "
            "a REAL boss guarding every stairway, each floor harder than the "
            "last. Entering raises your stakes to town 7, depth 20 - "
            "immediately, regardless of what happens down there.",
            "Descend", "Not today", [this, eternalTheme]() {
                Party& p = context_.party;
                if (stakesRaised(p.stakes, kTownCount, kMaxDepth)) {
                    p.stakes.penaltySteps = 0;  // a genuine raise, as M33 defines one
                }
                p.stakes.prevTown = kTownCount;
                p.stakes.prevDepth = kMaxDepth;
                content::LoadReport report;
                context_.saves.autosave(p, report);
                std::vector<dungeon::Dungeon> floors;
                floors.push_back(dungeon::generateEternalFloor(
                    seed_, 0, context_.content, eternalTheme, p.currentTown));
                stack().popState();  // leave the Guild
                stack().pushState(std::make_unique<DungeonState>(stack(), context_,
                                                                 std::move(floors)));
            }));
        return;
    }

    content::LoadReport report;
    context_.saves.autosave(context_.party, report);

    const std::string themeId = themeIds_.empty() ? "" : themeIds_[static_cast<std::size_t>(themeIndex_)];
    // M82: the run is its floors — one for the classic shape, four for the
    // descent (floors 1-3 end at an elite stair-gate; the boss waits below).
    std::vector<dungeon::Dungeon> floors = dungeon::generateFloors(
        seed_, depth_, context_.content, themeId, context_.party.currentTown, floors_);
    // M84 (Mind the Spoon): the omen perk's relic upgrade rides a pure hash of
    // the run seed AFTER generation, so the generator never sees party state
    // and a reload of this entry reproduces the same dungeon, omen included.
    applyGuildRelicOmen(floors, seed_, guildSpoonOmenPct(context_.party.guild));
    stack().popState();  // leave the Guild
    stack().pushState(std::make_unique<DungeonState>(stack(), context_, std::move(floors)));
}

void GuildState::handleInput(const Input& input) {
    // M88: the seed editor swallows everything while open. Same input contract
    // as party naming (M13): typed chars via nextChar, TextBackspace erases,
    // Confirm commits, Cancel abandons. An empty (or zero) buffer keeps the
    // old seed, so a stray Confirm can never produce a surprise run.
    if (seedEditing_) {
        for (int c = input.nextChar(); c > 0; c = input.nextChar()) {
            seedInput_.appendCodepoint(c);
        }
        if (input.navPressed(InputAction::TextBackspace)) {
            seedInput_.backspace();
        } else if (input.pressed(InputAction::Confirm)) {
            const std::uint64_t typed = parseSeedDigits(seedInput_.value());
            if (typed != 0) {
                seed_ = typed;
            }
            seedEditing_ = false;
            context_.audio.play(Sfx::Confirm);
        } else if (input.pressed(InputAction::Cancel)) {
            seedEditing_ = false;
            context_.audio.play(Sfx::Cancel);
        }
        return;
    }

    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
    }

    const int dir = (input.navPressed(InputAction::MoveRight) ? 1 : 0) -
                    (input.navPressed(InputAction::MoveLeft) ? 1 : 0);
    if (dir != 0) {
        if (menu_.cursor() == kTheme && !themeIds_.empty()) {
            const int n = static_cast<int>(themeIds_.size());
            themeIndex_ = ((themeIndex_ + dir) % n + n) % n;
            rebuild();  // reflect the new value inline immediately
        } else if (menu_.cursor() == kDepth) {
            if (floors_ == kEternalFloors) {
                context_.audio.play(Sfx::Cancel);  // M105: Eternal fixes depth at 20
            } else {
                depth_ = std::clamp(depth_ + dir, 1, kMaxDepth);
                rebuild();
            }
        } else if (menu_.cursor() == kFloors) {
            // M92: three shapes — the classic level, the descent, and the long
            // descent. M105: town 7 adds a fourth, the Eternal endless descent.
            // Left/Right walk the cycle in either direction.
            const bool eternalHere = context_.party.currentTown >= kTownCount;
            if (dir > 0) {
                floors_ = floors_ == 1 ? 4
                          : (floors_ == 4 ? 20
                                          : (floors_ == 20 && eternalHere ? kEternalFloors : 1));
            } else {
                floors_ = floors_ == 1 ? (eternalHere ? kEternalFloors : 20)
                          : (floors_ == kEternalFloors ? 20 : (floors_ == 20 ? 4 : 1));
            }
            rebuild();
        } else if (menu_.cursor() == kSeed) {
            seed_ = randomSeed();  // M88: the old "New Seed", now the row's Adjust
        }
    }

    // M33: the first time the configured run would incur a stakes penalty, teach
    // it (fired here, never from render). maybeTutorialPrompt shows it at most once.
    if (stakesPenaltyPct(context_.party.stakes, context_.party.currentTown, depth_) > 0) {
        maybeTutorialPrompt(stack(), context_, tutorial::kFirstPenalty);
    }

    if (input.pressed(InputAction::Cancel)) {
        stack().popState();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        switch (menu_.cursor()) {
            case kEnter: enterDungeon(); break;
            case kGuildBoss:
                // M84: the audience must be earned — a 4-floor clear in THIS
                // town. The banner under the panel already says so.
                if (guildRecord(context_.party.guild, context_.party.currentTown).unlocked) {
                    stack().pushState(std::make_unique<CastleChallengeState>(
                        stack(), context_, CastleChallenge::GuildBoss,
                        context_.party.currentTown));
                } else {
                    context_.audio.play(Sfx::Cancel);
                }
                break;
            case kSeed:
                // M88: type a seed by hand. Prefilled with the current one so
                // small edits (share a friend's seed, tweak a digit) are cheap.
                seedInput_.setValue(std::to_string(static_cast<unsigned long long>(seed_)));
                seedEditing_ = true;
                break;
            case kBack: stack().popState(); break;
            default: break;  // Theme/Depth are adjusted with Left/Right
        }
    }
}

void GuildState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ui::drawSceneBackground(context_.resources, "bg.guild", p.canvas,
                            context_.virtualWidth, context_.virtualHeight, context_.party.currentTown);

    ui::drawHeaderBand("Guild", w, p.crystal);
    ui::drawTextCentered("Choose a dungeon - entering autosaves.", w / 2, 28, style::kFontBody,
                         p.textDim);

    // Control panel: the CTA row on top, then the two stepper rows, then the
    // plain rows. The menu model still owns navigation. M84: the paddings are
    // trimmed by 2px so the Guild Boss row fits above the seed chip and the
    // banner without crowding the footer.
    const int px = 90;
    const int py = 42;
    const int pw = 246;
    const int ph = 140;  // M82 Floors + M84 Guild Boss rows
    ui::drawFrame(px, py, pw, ph, ui::FrameStyle::Standard);
    const int rowX = px + 22;

    // "Enter Dungeon" — the dominant call to action.
    const int enterY = py + 8;
    const bool enterFocused = menu_.cursor() == kEnter;
    ui::drawFrame(px + 10, enterY - 4, pw - 20, 22,
                  enterFocused ? ui::FrameStyle::Reward : ui::FrameStyle::Raised);
    ui::drawTextCentered("Enter Dungeon", px + pw / 2, enterY, 12,
                         enterFocused ? p.cursor : p.text);
    if (enterFocused) {
        ui::drawChevron(px + 18, enterY + 2, p.cursor, ui::motionPhase());
    }
    ui::drawDivider(px + 10, py + 30, pw - 20);

    // Stepper rows: label, left arrow, framed value capsule, right arrow.
    const auto stepperRow = [&](int index, const char* label, const std::string& value, int y) {
        const bool focused = menu_.cursor() == index;
        if (focused) {
            ui::drawSelectionSlab(rowX - 12, y - 2, pw - 40, 15);
            ui::drawChevron(rowX - 9, y + 1, p.cursor, ui::motionPhase());
        }
        ui::drawText(label, rowX, y, style::kFontMenu, focused ? p.cursor : p.text);
        const int capW = 104;
        const int capX = px + pw - 22 - capW;
        ui::drawStepArrow(capX - 11, y + 2, -1, focused);
        ui::drawStepArrow(capX + capW + 6, y + 2, +1, focused);
        DrawRectangle(capX, y - 1, capW, 13, p.ink);
        DrawRectangle(capX + 1, y, capW - 2, 11, p.chipFill);
        if (focused) {
            DrawRectangleLines(capX, y - 1, capW, 13, p.rowBorder);
        }
        const int vw = ui::measureText(value, style::kFontBody);
        ui::drawTextFitted(value, capX + (capW - vw) / 2, y + 1, capW - 6, style::kFontBody,
                           focused ? p.cursor : p.text, "guild.capsule");
    };
    stepperRow(kTheme, "Theme", currentThemeName(), py + 38);
    stepperRow(kDepth, "Depth",
               floors_ == kEternalFloors ? "20 (fixed)" : std::to_string(depth_), py + 56);
    // M82/M92: the run's shape — one classic level, or four (or twenty) flat
    // floors with elite stair-gates and the boss at the bottom. M105: town 7
    // adds the Eternal endless descent (no score; a boss on every floor).
    stepperRow(kFloors, "Floors",
               floors_ == 1
                   ? "1"
                   : (floors_ == 4 ? "4 (boss below)"
                                   : (floors_ == kEternalFloors ? "Eternal" : "20 (boss below)")),
               py + 74);
    // M105: the endless record rides under the panel at its home town.
    if (context_.party.currentTown >= kTownCount && context_.party.eternalBestFloors > 0) {
        ui::drawTextCentered(TextFormat("Eternal best: %d floors",
                                        context_.party.eternalBestFloors),
                             px + pw / 2, py + ph + 4, style::kFontSmall, p.gold);
    }

    // Plain rows.
    const auto plainRow = [&](int index, const char* label, int y, bool dim = false) {
        const bool focused = menu_.cursor() == index;
        if (focused) {
            ui::drawSelectionSlab(rowX - 12, y - 2, ui::measureText(label, style::kFontMenu) + 24,
                                  15);
            ui::drawChevron(rowX - 9, y + 1, p.cursor, ui::motionPhase());
        }
        ui::drawText(label, rowX, y, style::kFontMenu,
                     focused ? p.cursor : (dim ? p.textDim : p.text));
    };
    // M84: the Master's row — dim while the audience is unearned, carrying the
    // best-turns record once it falls. The banner below explains its state
    // whenever the row is focused.
    const GuildTownRecord& guildRec =
        guildRecord(context_.party.guild, context_.party.currentTown);
    plainRow(kGuildBoss, "Fight the Guild Boss", py + 92, !guildRec.unlocked);
    if (guildRec.defeated()) {
        const std::string best = "best " + std::to_string(guildRec.bestTurns);
        ui::drawText(best.c_str(), px + pw - 22 - ui::measureText(best, style::kFontBody),
                     py + 93, style::kFontBody, p.gold);
    }
    // M88: the Seed row — value capsule on the row that changes it, like Theme
    // and Depth, but wider (a uint64 runs to 20 digits). Left/Right rerolls
    // (the retired "New Seed" row's job), Confirm opens the digit editor.
    {
        const int y = py + 108;
        const bool focused = menu_.cursor() == kSeed;
        if (focused) {
            ui::drawSelectionSlab(rowX - 12, y - 2, pw - 40, 15);
            ui::drawChevron(rowX - 9, y + 1, p.cursor, ui::motionPhase());
        }
        ui::drawText("Seed", rowX, y, style::kFontMenu, focused ? p.cursor : p.text);
        const int capW = 140;
        const int capX = px + pw - 22 - capW;
        ui::drawStepArrow(capX - 11, y + 2, -1, focused);
        ui::drawStepArrow(capX + capW + 6, y + 2, +1, focused);
        DrawRectangle(capX, y - 1, capW, 13, p.ink);
        DrawRectangle(capX + 1, y, capW - 2, 11, p.chipFill);
        if (focused) {
            DrawRectangleLines(capX, y - 1, capW, 13, p.rowBorder);
        }
        const std::string seedText = std::to_string(static_cast<unsigned long long>(seed_));
        const int vw = ui::measureText(seedText, style::kFontBody);
        const int tx = capX + (vw < capW - 6 ? (capW - vw) / 2 : 3);
        ui::drawTextFitted(seedText, tx, y + 1, capW - 6, style::kFontBody,
                           focused ? p.cursor : p.text, "guild.seed");
    }
    plainRow(kBack, "Back", py + 124);

    // M33 stakes forewarning: the penalty this run (currentTown + chosen depth)
    // will incur, updating live as Depth changes; matches what completeDungeon
    // applies. Honest and shown before entering (M19). M84: while the Guild
    // Boss row is focused this line speaks for the gauntlet instead — the
    // stakes belong to the run being configured, not to that fight.
    const int pen = stakesPenaltyPct(context_.party.stakes, context_.party.currentTown, depth_);
    // M88: the seed chip merged into the Seed row, freeing this strip — the
    // banner moved up to py+ph+8 (was +24), so even a two-line wrap ends well
    // above the footer at h-16 (the owner-reported clip). The capture scene
    // pins the worst case (-99% text) against the overflow lint.
    const int bannerY = py + ph + 8;
    if (menu_.cursor() == kGuildBoss) {
        if (!guildRec.unlocked) {
            ui::drawBanner(ui::BannerKind::Danger,
                           "Clear a 4-floor dungeon here first.",
                           70, bannerY, w - 140, "guild.boss.status");
        } else if (!guildRec.defeated()) {
            ui::drawBanner(ui::BannerKind::Reward,
                           "Five foes, then the Master. No rest.",
                           70, bannerY, w - 140, "guild.boss.status");
        } else {
            ui::drawBanner(ui::BannerKind::Success,
                           TextFormat("The Master fell in %d turns.", guildRec.bestTurns),
                           70, bannerY, w - 140, "guild.boss.status");
        }
    } else if (pen > 0) {
        ui::drawBanner(ui::BannerKind::Danger,
                       TextFormat("Stakes penalty: -%d%% - raise town or depth to clear it.", pen),
                       70, bannerY, w - 140, "guild.stakes");
    } else {
        const char* okText = "No stakes penalty - this run raises the stakes.";
        const int tw = ui::measureText(okText, style::kFontBody);
        const int tx = w / 2 - tw / 2;
        DrawRectangle(tx - 10, bannerY + 4, 2, 2, p.success);  // stepped diamond, shape
        DrawRectangle(tx - 12, bannerY + 6, 6, 2, p.success);  // + color double signal
        DrawRectangle(tx - 10, bannerY + 8, 2, 2, p.success);
        ui::drawText(okText, tx, bannerY + 2, style::kFontBody, p.success);
    }

    // M88: the manual seed editor — a small modal over the panel. Digits only
    // (ui::TextFilter::Digits, 20 chars — the uint64 ceiling); Confirm keeps a
    // non-empty value, Cancel or an empty buffer keeps the old seed.
    if (seedEditing_) {
        const int mw = 208;
        const int mh = 56;
        const int mx = w / 2 - mw / 2;
        const int my = 88;
        ui::drawFrame(mx, my, mw, mh, ui::FrameStyle::Reward);
        ui::drawTextCentered("Enter Seed", mx + mw / 2, my + 6, style::kFontMenu, p.gold);
        DrawRectangle(mx + 10, my + 22, mw - 20, 14, p.ink);
        DrawRectangle(mx + 11, my + 23, mw - 22, 12, p.chipFill);
        const std::string& buf = seedInput_.value();
        ui::drawTextFitted(buf, mx + 14, my + 25, mw - 28, style::kFontBody, p.text,
                           "guild.seed.edit");
        if (ui::motionPhase() == 0 && !seedInput_.full()) {
            const int bw = std::min(ui::measureText(buf, style::kFontBody), mw - 28);
            DrawRectangle(mx + 14 + bw + 1, my + 25, 4, 9, p.cursor);
        }
        ui::drawTextCentered("Empty keeps the old seed.", mx + mw / 2, my + 42, style::kFontBody,
                             p.textDim);
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    if (seedEditing_) {
        // No Cancel hint: the default Cancel key IS Backspace, which the
        // editor consumes as Erase (the PartyCreation precedent) — showing
        // both would advertise a key that erases instead of cancelling. The
        // modal's own "Empty keeps the old seed." line covers backing out.
        ui::drawFooterHints(
            {{input::primaryLabel(map, InputAction::Confirm, device), "OK"},
             {input::primaryLabel(map, InputAction::TextBackspace, device), "Erase"}},
            w, h, "guild.footer");
    } else {
        ui::drawFooterHints(
            {{input::primaryLabel(map, InputAction::MoveLeft, device) + "/" +
                  input::primaryLabel(map, InputAction::MoveRight, device),
              "Adjust"},
             {input::primaryLabel(map, InputAction::Confirm, device), "Select"},
             {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
            w, h, "guild.footer");
    }
}

}  // namespace cd
