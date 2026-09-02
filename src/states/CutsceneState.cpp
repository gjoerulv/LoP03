#include "states/CutsceneState.hpp"

#include <algorithm>
#include <utility>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/Cutscenes.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "render/SpriteDraw.hpp"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {

// Stage geometry (426x240): actors stand on the floor line, the dialogue
// panel owns the bottom band, and the sky above stays uncluttered.
// M100 (owner request): the stage moved UP (150 -> 106) so the panel below can
// grow to six body lines — the longest authored beat now fits unscrolled.
constexpr int kFloorY = 106;         // feet line for every actor
constexpr int kPanelX = 8;
constexpr int kPanelW = 410;
constexpr int kChoiceBoxW = 300;     // the Stranger's offer box (owner fix 2026-08-17)
constexpr int kPartyX = 44;          // first member's center
constexpr int kPartySpacing = 34;
constexpr int kGooseX = 262;         // the storyteller's spot, right of center
constexpr int kKingX = 330;          // staged guests, behind/right of the goose
constexpr int kDragonX = 386;

// The goose's stage act as micro-offsets on the sanctioned motion clocks
// (M46: layout never moves; actors may nudge like the chevron does). Both
// clocks are deterministic, so captures and reduced-effect settings see a
// stable pose.
void gooseEmoteOffset(const std::string& emote, int& dx, int& dy) {
    dx = 0;
    dy = 0;
    if (emote == "waddle") {
        dx = ui::motionPhase() != 0 ? 1 : -1;
    } else if (emote == "jump") {
        dy = ui::motionPhase3() == 2 ? -3 : 0;
    } else if (emote == "panic") {
        dx = ui::motionPhase() != 0 ? 1 : -1;
        dy = ui::motionPhase3() == 1 ? -2 : 0;
    } else {  // idle: the gentlest bob
        dy = ui::motionPhase3() == 2 ? -1 : 0;
    }
}

}  // namespace

CutsceneState::CutsceneState(StateStack& stack, AppContext& context, std::string sceneId,
                             bool replay)
    : GameState(stack), context_(context), sceneId_(std::move(sceneId)), replay_(replay) {
    def_ = context_.content.findCutscene(sceneId_);
    if (def_ != nullptr) {
        std::vector<ui::MenuItem> items;
        for (const content::CutsceneOption& o : def_->options) {
            items.push_back({o.label, true});
        }
        choiceMenu_.setItems(std::move(items));
        loadBeat();
    }
}

const content::CutsceneBeat* CutsceneState::currentBeat() const {
    if (def_ == nullptr || def_->beats.empty()) {
        return nullptr;
    }
    const int last = static_cast<int>(def_->beats.size()) - 1;
    return &def_->beats[static_cast<std::size_t>(std::clamp(beatIndex_, 0, last))];
}

void CutsceneState::loadBeat() {
    const content::CutsceneBeat* beat = currentBeat();
    if (beat == nullptr) {
        return;
    }
    panelSpeaker_ = game::cutsceneResolveTokens(beat->speaker, context_.party);
    panelText_ = game::cutsceneResolveTokens(beat->text, context_.party);
}

void CutsceneState::advanceBeat() {
    if (def_ == nullptr) {
        return;
    }
    if (beatIndex_ + 1 < static_cast<int>(def_->beats.size())) {
        ++beatIndex_;
        loadBeat();
    } else if (def_->options.empty()) {
        stack().popState();  // M100: a joke grants nothing, asks nothing — it just ends
    } else {
        enterChoice();
    }
}

void CutsceneState::enterChoice() {
    phase_ = Phase::Choice;
    // Owner fix 2026-08-17: the box sizes itself to the LONGEST keepsake
    // description (measured wrapped lines, capped defensively), so no
    // heirloom's own words are cut to a two-line preview.
    choiceDetailLines_ = 2;
    if (def_ != nullptr) {
        for (const content::CutsceneOption& o : def_->options) {
            if (const content::ItemDef* it = context_.content.findItem(o.heirloomId)) {
                // Owner request 2026-08-28: the keepsake's name rides its own
                // icon + gold tag line; only the description wraps below it.
                const int lines = 1 + static_cast<int>(
                    ui::wrapText(it->description, kChoiceBoxW - 28,
                                 ui::style::kFontBody, ui::raylibMeasure())
                        .size());
                choiceDetailLines_ = std::max(choiceDetailLines_, std::min(lines, 6));
            }
        }
    }
    context_.audio.play(Sfx::Move);
}

void CutsceneState::commitChoice() {
    const int cursor = choiceMenu_.cursor();
    if (def_ == nullptr || cursor < 0 || cursor >= static_cast<int>(def_->options.size())) {
        return;
    }
    chosen_ = cursor;
    const content::CutsceneOption& o = def_->options[static_cast<std::size_t>(cursor)];
    // First real pick grants the keepsake; replays (finale NPC, debug play)
    // find a recorded choice — or the replay flag — and only retell the tale.
    if (!replay_ && game::cutsceneChoiceFor(context_.party, sceneId_).empty()) {
        context_.party.inventory.add(o.heirloomId, 1);
        game::recordCutsceneChoice(context_.party, sceneId_, o.heirloomId);
        context_.audio.play(Sfx::Chest);
    } else {
        context_.audio.play(Sfx::Confirm);
    }
    panelSpeaker_ = game::cutsceneResolveTokens(o.responseSpeaker, context_.party);
    panelText_ = game::cutsceneResolveTokens(o.responseText, context_.party);
    phase_ = Phase::Response;
}

void CutsceneState::handleInput(const Input& input) {
    if (def_ == nullptr) {
        stack().popState();  // unauthored scene: never traps the player
        return;
    }
    switch (phase_) {
        case Phase::Beats:
            if (input.navPressed(InputAction::MoveUp) && bodyView_.scrollBy(-1)) {
                context_.audio.play(Sfx::Move);
            }
            if (input.navPressed(InputAction::MoveDown) && bodyView_.scrollBy(1)) {
                context_.audio.play(Sfx::Move);
            }
            if (input.pressed(InputAction::Confirm)) {
                context_.audio.play(Sfx::Confirm);
                advanceBeat();
            } else if (input.pressed(InputAction::Cancel)) {
                context_.audio.play(Sfx::Move);
                phase_ = Phase::SkipAsk;
            }
            break;
        case Phase::SkipAsk:
            if (input.pressed(InputAction::Confirm)) {
                context_.audio.play(Sfx::Confirm);
                if (def_->options.empty()) {
                    stack().popState();  // M100: no choice to land on — just leave
                } else {
                    enterChoice();  // skip lands ON the choice — never past it
                }
            } else if (input.pressed(InputAction::Cancel)) {
                context_.audio.play(Sfx::Cancel);
                phase_ = Phase::Beats;
            }
            break;
        case Phase::Choice:
            if (input.navPressed(InputAction::MoveUp)) {
                choiceMenu_.moveUp();
            }
            if (input.navPressed(InputAction::MoveDown)) {
                choiceMenu_.moveDown();
            }
            if (input.pressed(InputAction::Confirm)) {
                commitChoice();
            }
            // Cancel deliberately does nothing: the choice must be answered.
            break;
        case Phase::Response:
            if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
                context_.audio.play(Sfx::Confirm);
                stack().popState();
            }
            break;
    }
}

void CutsceneState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();

    // The stage: a deep night sky over a packed-earth floor band. Flat fills
    // (no textures needed), readable in high contrast, quiet behind actors.
    DrawRectangle(0, 0, w, h, Color{13, 12, 26, 255});
    DrawRectangle(0, kFloorY + 2, w, h - kFloorY - 2, Color{34, 28, 40, 255});
    DrawRectangle(0, kFloorY + 1, w, 1, Color{58, 50, 66, 255});  // horizon keyline

    const content::CutsceneBeat* beat = currentBeat();
    const bool showGuests = beat != nullptr && phase_ != Phase::Choice;

    // Staged guests first (behind the goose), dimmed: memories, not fights.
    // M98: the staged King is THE Hollow King (the castle boss), not the
    // hollow_sovereign dungeon boss the finale mistakenly showed.
    if (showGuests && beat->kingOnStage) {
        if (!render::drawTextureCentered(context_.resources, "boss.the_hollow_king.battle",
                                         static_cast<float>(kKingX),
                                         static_cast<float>(kFloorY - 16),
                                         Color{190, 190, 205, 255})) {
            DrawRectangle(kKingX - 12, kFloorY - 34, 24, 34, Color{90, 90, 120, 255});
        }
    }
    if (showGuests && beat->dragonOnStage) {
        if (!render::drawTextureCentered(context_.resources, "boss.the_dragon.battle",
                                         static_cast<float>(kDragonX),
                                         static_cast<float>(kFloorY - 18),
                                         Color{190, 190, 205, 255})) {
            DrawRectangle(kDragonX - 16, kFloorY - 38, 32, 38, Color{130, 80, 90, 255});
        }
    }

    // The party, standing left, in roster order.
    int px = kPartyX;
    for (const Character& m : context_.party.members) {
        if (!render::drawTextureCentered(context_.resources, "actor." + m.classId + ".battle",
                                         static_cast<float>(px),
                                         static_cast<float>(kFloorY - 10))) {
            DrawRectangle(px - 6, kFloorY - 18, 12, 18, Color{90, 130, 190, 255});
        }
        px += kPartySpacing;
    }

    // The hooded goose, mid-stage, acting its emote on the sanctioned clocks.
    {
        int dx = 0;
        int dy = 0;
        gooseEmoteOffset(beat != nullptr ? beat->emote : "idle", dx, dy);
        if (!render::drawTextureCentered(context_.resources, "actor.hooded_goose.stage",
                                         static_cast<float>(kGooseX + dx),
                                         static_cast<float>(kFloorY - 12 + dy))) {
            DrawRectangle(kGooseX - 8 + dx, kFloorY - 24 + dy, 16, 24, Color{120, 100, 150, 255});
        }
    }

    // Bottom dialogue panel: speaker plaque line + height-capped scrolling
    // body (M87 policy A — a translated beat of any length stays reachable).
    if (phase_ == Phase::Beats || phase_ == Phase::SkipAsk || phase_ == Phase::Response) {
        const int textW = kPanelW - 2 * style::kPad - ui::kScrollGutterW;
        bodyView_.setContent(panelText_, textW, style::kFontBody, ui::raylibMeasure());
        // M100: six lines fit under the raised stage, and the longest shipped
        // beat wraps to five — scrolling remains only as the translation
        // safety net (M87 policy A), no longer the routine reading mode.
        const int capLines = 6;
        bodyView_.setVisibleLines(std::clamp(bodyView_.lineCount(), 1, capLines));
        const int bodyH = bodyView_.visibleLines() * ui::lineHeight(style::kFontBody);
        const int chromeH = style::kPad + style::kFontSmall + 5 + 4 + style::kFontSmall +
                            style::kPad;
        const int panelH = chromeH + bodyH;
        const int panelY = h - style::kSafeMargin - panelH;
        ui::drawFrame(kPanelX, panelY, kPanelW, panelH, ui::FrameStyle::Raised);
        int ty = panelY + style::kPad;
        ui::drawText(("- " + panelSpeaker_ + " -").c_str(), kPanelX + style::kPad, ty,
                     style::kFontSmall, p.gold);
        ty += style::kFontSmall + 5;
        ty = ui::drawTextViewport(bodyView_, kPanelX + style::kPad, ty, p.text);
        ty += 4;
        const InputMap& map = context_.input.map();
        const ActiveDevice device = context_.input.activeDevice();
        std::string hint =
            input::prompt(map, InputAction::Confirm,
                          device, phase_ == Phase::Response ? "Close" : "Next");
        if (phase_ != Phase::Response) {
            hint += "   " + input::prompt(map, InputAction::Cancel, device, "Skip");
        }
        if (bodyView_.scrollable()) {
            hint = input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
                   input::primaryLabel(map, InputAction::MoveDown, device) + " Scroll   " + hint;
        }
        ui::drawTextCentered(hint.c_str(), kPanelX + kPanelW / 2, ty, style::kFontSmall,
                             p.textHint);
    }

    if (phase_ == Phase::SkipAsk) {
        ui::drawModalDim(w, h);
        const int boxW = 220;
        const int boxH = 62;
        const int boxX = w / 2 - boxW / 2;
        const int boxY = h / 2 - boxH / 2;
        ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Raised);
        ui::drawTextCentered("Skip the scene?", w / 2, boxY + 10, style::kFontBody, p.text);
        ui::drawTextCentered(def_ != nullptr && def_->options.empty()
                                 ? "It was short anyway."  // M100: a joke asks nothing
                                 : "The choice will still be asked.",
                             w / 2, boxY + 24, style::kFontSmall, p.textDim);
        const InputMap& map = context_.input.map();
        const ActiveDevice device = context_.input.activeDevice();
        const std::string hint =
            input::prompt(map, InputAction::Confirm, device, "Skip") + "   " +
            input::prompt(map, InputAction::Cancel, device, "Stay");
        ui::drawTextCentered(hint.c_str(), w / 2, boxY + boxH - 16, style::kFontSmall, p.textHint);
    }

    if (phase_ == Phase::Choice && def_ != nullptr) {
        ui::drawModalDim(w, h);
        const int boxW = kChoiceBoxW;
        // Owner fix 2026-08-17: the box grows past its old two-line detail
        // budget to fit the longest keepsake description (enterChoice measures).
        const int extraDetail =
            (choiceDetailLines_ - 2) * ui::lineHeight(style::kFontBody);
        const int boxH = 96 + static_cast<int>(choiceMenu_.size()) * 16 + extraDetail;
        const int boxX = w / 2 - boxW / 2;
        const int boxY = h / 2 - boxH / 2;
        ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Reward);
        ui::drawTitlePlaque("The Stranger \"P\" Offers", w / 2, boxY - 10, 12);  // M100
        ui::drawTextPreview(game::cutsceneResolveTokens(def_->question, context_.party),
                            boxX + 14, boxY + 16, boxW - 28, style::kFontBody, p.text, 2);
        ui::drawMenu(choiceMenu_, boxX + 28, boxY + 48, 16, 12, p.text, p.disabled, p.cursor);

        // The highlighted keepsake's own words under the list, in full.
        const int cursor = choiceMenu_.cursor();
        const int detailY = boxY + 52 + static_cast<int>(choiceMenu_.size()) * 16;
        if (cursor >= 0 && cursor < static_cast<int>(def_->options.size())) {
            const content::CutsceneOption& o =
                def_->options[static_cast<std::size_t>(cursor)];
            if (const content::ItemDef* it = context_.content.findItem(o.heirloomId)) {
                // Owner request 2026-08-28: the keepsake leads with its M81
                // icon + name in the reward gold — the one gear-name
                // convention — and the description keeps its own dim lines.
                ui::drawGearNameTag(context_.resources, content::gearIconTextureId(*it),
                                    it->name, boxX + 14, detailY, style::kFontBody, p.gold);
                ui::drawTextPreview(it->description, boxX + 14,
                                    detailY + ui::lineHeight(style::kFontBody), boxW - 28,
                                    style::kFontBody, p.textDim, choiceDetailLines_ - 1);
            }
        }
    }
}

#ifdef CRYSTAL_CAPTURE
void CutsceneState::captureShowBeat(int index) {
    if (def_ == nullptr) {
        return;
    }
    beatIndex_ = std::clamp(index, 0, static_cast<int>(def_->beats.size()) - 1);
    phase_ = Phase::Beats;
    loadBeat();
}

void CutsceneState::captureShowChoice() {
    if (def_ != nullptr) {
        enterChoice();  // measures the detail budget exactly as play does
    }
}
#endif

}  // namespace cd
