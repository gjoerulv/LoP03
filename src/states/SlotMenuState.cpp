#include "states/SlotMenuState.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "game/IronMan.hpp"
#include "game/PlayTime.hpp"
#include "game/Profile.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "render/PartyHop.hpp"           // M127
#include "resource/ResourceManager.hpp"  // M127: the members' sprites
#include "states/StateStack.hpp"
#include "states/TownState.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
// M123: one summary() per slot per rebuild feeds the label, the King title and
// the play-time clock (it used to be loaded once for each).
std::string slotLabel(save::SaveSlot slot, const std::optional<save::SlotSummary>& s) {
    std::string label = save::slotDisplayName(slot);
    if (s) {
        // M67: the party is always four strong, so the old "party 4" column
        // said nothing — level and gold carry the slot's identity.
        label += TextFormat("  -  Lv.%d  %dg", s->highestLevel, s->gold);
    } else {
        label += "  -  (empty)";
    }
    return label;
}

constexpr int kRowX = 56;
// M123: the play-time clock ("HHH:MM:SS", game/PlayTime.hpp) is right-aligned
// here, at body size; the label's fitted width stops short of it.
constexpr int kClockRight = 42;  // from the right screen edge (M127: was 48)
constexpr int kClockGap = 8;
// M127 (owner request 2026-09-20): the slot's party stands left of the clock,
// centred in the row - the members' 24px battle sprites at 1x, stepped a
// little closer than their canvas (the margins are empty) so four of them
// and the widest label still share the row. The highlighted slot's party
// hops to the victory screen's beat (render/PartyHop.hpp), scaled down to
// the row.
constexpr int kSpriteSize = 24;
constexpr int kSpriteStep = 20;
constexpr int kSpriteLabelGap = 6;
constexpr int kMaxSprites = 4;
constexpr float kHopScale = 0.3f;
// M53: tighter static layout so all six Load rows (autosave + five manual), each
// able to carry a King-title second line, fit at 426x240 without scrolling.
constexpr int kRowsY = 52;
constexpr int kRowH = 24;
}  // namespace

SlotMenuState::SlotMenuState(StateStack& stack, AppContext& context, SlotMenuMode mode)
    : GameState(stack), context_(context), mode_(mode) {}

void SlotMenuState::onEnter() { rebuild(); }

void SlotMenuState::rebuild() {
    slots_.clear();
    titles_.clear();
    playSeconds_.clear();
    parties_.clear();
    std::vector<ui::MenuItem> items;
    ironManRefusal_ = mode_ == SlotMenuMode::Save && context_.party.ironMan;

    if (mode_ == SlotMenuMode::Load) {
        slots_ = {save::SaveSlot::Auto,    save::SaveSlot::Manual1, save::SaveSlot::Manual2,
                  save::SaveSlot::Manual3, save::SaveSlot::Manual4, save::SaveSlot::Manual5};
    } else {
        // The autosave slot is not manually writable.
        slots_ = {save::SaveSlot::Manual1, save::SaveSlot::Manual2, save::SaveSlot::Manual3,
                  save::SaveSlot::Manual4, save::SaveSlot::Manual5};
    }

    for (save::SaveSlot slot : slots_) {
        const bool exists = context_.saves.exists(slot);
        // Can't load empty; an Iron Man party can't save at all (M123).
        const bool enabled =
            !ironManRefusal_ && ((mode_ == SlotMenuMode::Save) || exists);
        const std::optional<save::SlotSummary> summary = context_.saves.summary(slot);
        items.push_back({slotLabel(slot, summary), enabled});
        // M40: a party that has beaten the King carries a visible title, drawn
        // on its own line under the slot row rather than appended to it.
        titles_.push_back(summary ? summary->kingTitle : std::string{});
        playSeconds_.push_back(summary ? summary->playSeconds : -1);
        parties_.push_back(summary ? summary->members
                                   : std::vector<save::SlotSummary::Member>{});  // M127
    }
    const int previous = menu_.cursor();
    menu_.setItems(std::move(items));
    menu_.setCursor(previous);
    if (ironManRefusal_) {
        message_ = ironman::kSaveRefusal;
    }
}

void SlotMenuState::confirmSelection() {
    if (!menu_.currentEnabled() || slots_.empty()) {
        return;
    }
    const save::SaveSlot slot = slots_[static_cast<std::size_t>(menu_.cursor())];
    content::LoadReport report;

    if (mode_ == SlotMenuMode::Save) {
        // Destructive-action confirmation (M22): overwriting an existing
        // save requires a second Confirm on the same slot.
        if (context_.saves.exists(slot) && pendingOverwrite_ != menu_.cursor()) {
            pendingOverwrite_ = menu_.cursor();
            message_ = std::string("Overwrite ") + std::string(save::slotDisplayName(slot)) +
                       "? Confirm again to overwrite.";
            return;
        }
        pendingOverwrite_ = -1;
        if (context_.saves.save(slot, context_.party, report)) {
            message_ = std::string("Saved to ") + save::slotDisplayName(slot);
        } else {
            message_ = "Save failed (see log)";
        }
        rebuild();
    } else {
        if (context_.saves.load(slot, context_.party, report)) {
            // M45: a party that already beat the King unlocks the reward classes
            // retroactively — a player who won before this build should not have
            // to win again.
            if (context_.party.castleRecords.kingDefeated) {
                context_.profile.recordKingDefeated();
            }
            stack().clearStates();
            stack().pushState(std::make_unique<TownState>(stack(), context_));
        } else {
            message_ = "Load failed: save is missing or invalid";
        }
    }
}

void SlotMenuState::handleInput(const Input& input) {
    if (input.navPressed(InputAction::MoveUp)) {
        menu_.moveUp();
        disarmOverwrite();
    }
    if (input.navPressed(InputAction::MoveDown)) {
        menu_.moveDown();
        disarmOverwrite();
    }
    if (input.pressed(InputAction::Confirm)) {
        confirmSelection();
    }
    if (input.pressed(InputAction::Cancel)) {
        if (pendingOverwrite_ >= 0) {
            disarmOverwrite();  // keep the save; stay on the screen
            return;
        }
        stack().popState();
    }
}

void SlotMenuState::update(float dt) { time_ += dt; }

void SlotMenuState::disarmOverwrite() {
    if (pendingOverwrite_ >= 0) {
        pendingOverwrite_ = -1;
        message_.clear();
    }
}

void SlotMenuState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);

    const char* title = mode_ == SlotMenuMode::Save ? "Save Game" : "Load Game";
    ui::drawTitlePlaque(title, w / 2, 14, 16);

    // Rows are drawn here rather than via drawMenu so a slot's King title gets
    // its own line: appended to the label it ran straight off the screen.
    const std::vector<ui::MenuItem>& items = menu_.items();
    // Frame sized from the ACTUAL row count (M53): fits the six Load rows and no
    // longer overshoots in Save mode, where there are fewer.
    const int rows = static_cast<int>(items.size());
    ui::drawFrame(32, 40, w - 64, rows * kRowH + 18, ui::FrameStyle::Standard);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const int y = kRowsY + static_cast<int>(i) * kRowH;
        const bool isCursor = static_cast<int>(i) == menu_.cursor();
        const bool hasTitle = i < titles_.size() && !titles_[i].empty();
        Color color = items[i].enabled ? p.text : p.disabled;
        if (isCursor && items[i].enabled) {
            color = p.cursor;
            // M67: a titled row's slab covers BOTH lines (the King title used
            // to hang half outside it); 25 tall still clears the next label.
            ui::drawSelectionSlab(kRowX - 14, y - 3, w - kRowX - 26,
                                  hasTitle ? kRowH + 1 : kRowH - 3);
            ui::drawChevron(kRowX - 11, y + 1, p.cursor, ui::motionPhase());
        }
        // M123: the clock first (it sets how far the label may run). The hour
        // digits the party has not reached yet are greyed; an empty slot has
        // no clock.
        int labelW = w - kRowX - 48;
        int rightEdge = w - kClockRight;  // M127: what is left of the row, right to left
        if (i < playSeconds_.size() && playSeconds_[i] >= 0) {
            const SlotPlayTime time = formatSlotPlayTime(playSeconds_[i]);
            const std::string grey = time.text.substr(0, static_cast<std::size_t>(time.greyChars));
            const std::string lit = time.text.substr(static_cast<std::size_t>(time.greyChars));
            const int fullW = ui::measureText(time.text, ui::style::kFontBody);
            const int clockX = w - kClockRight - fullW;
            if (!grey.empty()) {
                ui::drawText(grey, clockX, y + 1, ui::style::kFontBody, p.disabled);
            }
            ui::drawText(lit, w - kClockRight - ui::measureText(lit, ui::style::kFontBody), y + 1,
                         ui::style::kFontBody, items[i].enabled ? p.text : p.disabled);
            rightEdge = clockX - kClockGap;
            labelW = rightEdge - kRowX;
        }
        // M127: the party, left of the clock and centred on the row's slab.
        if (i < parties_.size() && !parties_[i].empty()) {
            const std::vector<save::SlotSummary::Member>& party = parties_[i];
            const int n = std::min(static_cast<int>(party.size()), kMaxSprites);
            const int blockW = (n - 1) * kSpriteStep + kSpriteSize;
            const int slabH = hasTitle ? kRowH + 1 : kRowH - 3;
            const int top = y - 3 + (slabH - kSpriteSize) / 2;
            const int left = rightEdge - blockW;
            const bool hopping = isCursor && items[i].enabled;
            for (int m = 0; m < n; ++m) {
                const save::SlotSummary::Member& member = party[static_cast<std::size_t>(m)];
                const std::string sprId = "actor." + member.classId + ".battle";
                if (!context_.resources.hasTexture(sprId)) {
                    continue;  // placeholder discipline: an unknown class draws nothing
                }
                const Texture2D& tex = context_.resources.texture(sprId);
                const int sx = left + m * kSpriteStep;
                if (member.fallen) {
                    // Lying down and dimmed, never hopping - the victory screen's rule.
                    DrawTexturePro(tex,
                                   Rectangle{0, 0, static_cast<float>(tex.width),
                                             static_cast<float>(tex.height)},
                                   Rectangle{static_cast<float>(sx + kSpriteSize / 2),
                                             static_cast<float>(top + kSpriteSize / 2 + 3),
                                             static_cast<float>(kSpriteSize),
                                             static_cast<float>(kSpriteSize)},
                                   Vector2{kSpriteSize / 2.0f, kSpriteSize / 2.0f}, 90.0f,
                                   Color{110, 110, 125, 255});
                    continue;
                }
                const int hop =
                    hopping ? static_cast<int>(render::partyHop(m, time_, kHopScale)) : 0;
                DrawTexture(tex, sx, top - hop,
                            items[i].enabled ? WHITE : Color{120, 120, 135, 255});
            }
            rightEdge = left - kSpriteLabelGap;
            labelW = rightEdge - kRowX;
        }
        ui::drawTextFitted(items[i].label, kRowX, y, labelW, 12, color, "slot.label");
        if (hasTitle) {
            ui::drawTextFitted(titles_[i], kRowX + 12, y + 12, w - kRowX - 60, 8,
                               p.gold, "slot.title");
        }
    }

    if (!message_.empty()) {
        // Below the deepest row a Load list can reach (6 slots, each able to
        // carry a title line: y=52..172, deepest title ~185, frame bottom ~202)
        // and clear of the footer. Overwrite arming is a consequence question, so
        // it rides the Danger banner.
        ui::drawBanner(pendingOverwrite_ >= 0 || ironManRefusal_ ? ui::BannerKind::Danger
                                                                  : ui::BannerKind::Success,
                       message_, 60, 204, w - 120, "slot.message");
    }
    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    if (ironManRefusal_) {
        // Nothing to confirm: the only way on is back.
        ui::drawFooterHints({{input::primaryLabel(map, InputAction::Cancel, device), "Back"}}, w,
                            h, "slot.footer");
        return;
    }
    ui::drawFooterHints(
        {{input::primaryLabel(map, InputAction::Confirm, device),
          mode_ == SlotMenuMode::Save ? "Save" : "Load"},
         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
        w, h, "slot.footer");
}

}  // namespace cd
