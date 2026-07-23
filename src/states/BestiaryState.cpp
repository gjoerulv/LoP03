#include "states/BestiaryState.hpp"

#include <algorithm>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "content/Enums.hpp"
#include "core/AppContext.hpp"
#include "game/Castle.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "render/SpriteDraw.hpp"
#include "states/BestiaryStats.hpp"
#include "states/StateStack.hpp"
#include "ui/TextLayout.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace style = ui::style;

namespace {
constexpr int kVisibleRows = 12;
constexpr int kListX = 24;
constexpr int kListW = 158;   // fits the longest boss name at the body font
constexpr int kRowsY = 42;
constexpr int kRowH = 14;
constexpr int kPanelX = 194;  // detail panel, just right of the list

// Foes the party has not met read as unknowns, the same way locked achievements
// do, so the roster's size is visible without spoiling what is in it.
const char* kUnknownName = "? ? ?";

std::vector<std::string> passiveNames(const std::vector<std::string>& ids,
                                      const content::ContentDatabase& db) {
    std::vector<std::string> out;
    for (const std::string& id : ids) {
        if (const content::PassiveDef* p = db.findPassive(id)) {
            out.push_back(p->name);
        }
    }
    return out;
}

// M48: element names as the player reads them, from the shared display table.
std::vector<std::string> elementNames(const std::vector<content::Element>& elements) {
    std::vector<std::string> out;
    for (content::Element e : elements) {
        out.push_back(content::elementDisplayName(e));
    }
    return out;
}

// "Fire, Holy" — affinities are short lists, so one joined line reads better
// than the passives' line-per-entry treatment.
std::string joined(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& p : parts) {
        if (!out.empty()) {
            out += ", ";
        }
        out += p;
    }
    return out;
}
}  // namespace

BestiaryState::BestiaryState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {
    const std::vector<std::string>& seen = context_.party.encountered;
    const auto known = [&seen](const std::string& id) {
        return std::find(seen.begin(), seen.end(), id) != seen.end();
    };
    // M52: the dungeon ceiling, derived once from the shipped composition + ladder
    // (570% at the shipped values), feeds the "at their strongest" scale below.
    const int castleFloor = castleFloorScalePct(context_.content);

    // The whole roster, enemies first then bosses, each alphabetical. An entry
    // keeps its slot when it is discovered, so the list never reshuffles.
    for (const auto& [id, def] : context_.content.enemies()) {
        Entry e;
        e.id = id;
        e.spriteId = "enemy." + id + ".battle";
        e.name = def.name;
        e.stats = def.stats;
        e.profile = std::string(content::toString(def.tier)) + " " + content::toString(def.role);
        for (content::EnemyTag t : def.tags) {
            e.profile += std::string(" ") + content::toString(t);
        }
        e.passives = passiveNames(def.passives, context_.content);
        e.weakTo = elementNames(def.affinity.weaknesses);    // M48
        e.immuneTo = elementNames(def.affinity.immunities);
        e.known = known(id);
        e.maxScalePct = foeMaxScalePct(false, def.bossOnly, false, castleFloor);  // M52
        entries_.push_back(std::move(e));
    }
    const std::size_t firstBoss = entries_.size();
    for (const auto& [id, def] : context_.content.bosses()) {
        Entry e;
        e.id = id;
        e.boss = true;
        e.spriteId = "boss." + id + ".battle";
        e.name = def.name;
        e.stats = def.stats;
        e.profile = std::string(content::toString(def.archetype)) + " boss";
        e.passives = passiveNames(def.passives, context_.content);
        e.weakTo = elementNames(def.affinity.weaknesses);    // M48
        e.immuneTo = elementNames(def.affinity.immunities);
        e.flavor = def.description;
        e.known = known(id);
        e.maxScalePct = foeMaxScalePct(true, false, id == kKingBossId, castleFloor);  // M52
        entries_.push_back(std::move(e));
    }
    const auto byName = [](const Entry& a, const Entry& b) { return a.name < b.name; };
    std::sort(entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>(firstBoss), byName);
    std::sort(entries_.begin() + static_cast<std::ptrdiff_t>(firstBoss), entries_.end(), byName);

    for (const Entry& e : entries_) {
        known_ += e.known ? 1 : 0;
    }
}

#ifdef CRYSTAL_CAPTURE
void BestiaryState::captureSelect(const std::string& id) {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].id == id) {
            cursor_ = static_cast<int>(i);
            scroll_.follow(static_cast<int>(entries_.size()), kVisibleRows, cursor_);
            return;
        }
    }
}
#endif

void BestiaryState::handleInput(const Input& input) {
    const int total = static_cast<int>(entries_.size());
    if (input.navPressed(InputAction::MoveUp) && cursor_ > 0) {
        --cursor_;
        scroll_.follow(total, kVisibleRows, cursor_);
    }
    if (input.navPressed(InputAction::MoveDown) && cursor_ + 1 < total) {
        ++cursor_;
        scroll_.follow(total, kVisibleRows, cursor_);
    }
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void BestiaryState::renderDetail(const Entry& e, int px, int pw) {
    const int textX = px + 52;               // right of the sprite well
    const int textW = px + pw - 12 - textX;  // name/profile column
    const int bodyW = pw - 24;               // full-width rows below the sprite
    const int bottom = context_.virtualHeight - style::kFooterHeight - 2;

    if (e.known) {
        render::drawTextureCentered(context_.resources, e.spriteId,
                                    static_cast<float>(px + 28), 62.0f);
    } else {
        ui::drawTextCentered("?", px + 28, 52, style::kFontScreenTitle, style::palette().disabled);
    }

    int ty = ui::drawTextWrapped(e.known ? e.name : kUnknownName, textX, 42, textW,
                                 style::kFontHeading, style::palette().text, "bestiary.detailname",
                                 2);
    ty = ui::drawTextWrapped(e.known ? e.profile : "Not yet encountered.", textX, ty, textW,
                             style::kFontSmall, style::palette().textDim, "bestiary.profile", 2);

    int y = std::max(ty + 4, 82);  // clears the 32px sprite well
    const content::StatBlock& s = e.stats;
    if (e.known) {
        // M52: base stats, then the strongest real-context values on ONE more
        // line, positionally matching the labels above (HP ATK MAG DEF SPD). The
        // context scale is foeMaxScalePct (regular ×5.70, guards/King ×5.00,
        // bosses ×5.80; the unbounded endless rush is excluded); the per-field
        // multiply is content::scaledStats, shared with buildBattle so the
        // numbers cannot lie. Kept to two lines — exactly the base pair's old
        // footprint — so the densest entry (the King, scene 41, whose long flavor
        // has no spare room) stays overflow-clean. Documented in game_design.md.
        const content::StatBlock m = content::scaledStats(s, e.maxScalePct);
        ui::drawTextFitted(TextFormat("HP %d ATK %d MAG %d DEF %d SPD %d", s.maxHp, s.attack,
                                      s.magic, s.defense, s.speed),
                           px + 12, y, bodyW, style::kFontBody, style::palette().textDim,
                           "bestiary.stats");
        y += 13;
        ui::drawTextFitted(TextFormat("max %d %d %d %d %d", m.maxHp, m.attack, m.magic, m.defense,
                                      m.speed),
                           px + 12, y, bodyW, style::kFontBody, style::palette().textHint,
                           "bestiary.statsmax");
    } else {
        ui::drawText("HP ?   ATK ?   MAG ?", px + 12, y, style::kFontBody,
                     style::palette().disabled);
        y += 13;
        ui::drawText("DEF ?   SPD ?", px + 12, y, style::kFontBody, style::palette().disabled);
    }
    y += 14;

    // Passives get one line each: a foe with three of them (the King) would
    // otherwise run off the panel on a single row.
    if (e.known && !e.passives.empty()) {
        ui::drawText(e.passives.size() > 1 ? "Passives" : "Passive", px + 12, y, style::kFontSmall,
                     style::palette().textDim);
        y += 11;
        for (const std::string& p : e.passives) {
            ui::drawTextFitted(p, px + 18, y, bodyW - 6, style::kFontBody,
                               style::palette().gold, "bestiary.passive");
            y += 11;
        }
        y += 4;
    }

    // M48: element affinities, revealed only for a foe the party has actually
    // met — an unknown gives nothing away, the same masking rule as its stats.
    if (e.known && (!e.weakTo.empty() || !e.immuneTo.empty())) {
        if (!e.weakTo.empty()) {
            ui::drawTextFitted("Weak: " + joined(e.weakTo), px + 12, y, bodyW,
                               style::kFontBody, style::palette().gold, "bestiary.weak");
            y += 11;
        }
        if (!e.immuneTo.empty()) {
            ui::drawTextFitted("Immune: " + joined(e.immuneTo), px + 12, y, bodyW,
                               style::kFontBody, style::palette().dangerText, "bestiary.immune");
            y += 11;
        }
        y += 4;
    }

    if (e.known && !e.flavor.empty()) {
        // Prefer the body font; drop to the caption font only when the flavor
        // would not otherwise fit the room the panel has left.
        const int room = bottom - y;
        const int bodyLines = room / ui::lineHeight(style::kFontBody);
        const bool fitsBody =
            static_cast<int>(
                ui::wrapText(e.flavor, bodyW, style::kFontBody, ui::raylibMeasure()).size()) <=
            bodyLines;
        const int font = fitsBody ? style::kFontBody : style::kFontSmall;
        const int lines = room / ui::lineHeight(font);
        if (lines > 0) {
            ui::drawTextWrapped(e.flavor, px + 12, y, bodyW, font, style::palette().text,
                                "bestiary.flavor", lines);
        }
    }
}

void BestiaryState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Bestiary", w, p.danger);

    const int total = static_cast<int>(entries_.size());
    ui::drawTextRight(TextFormat("%d / %d recorded", known_, total), kListX + kListW, 28,
                      style::kFontSmall, p.textDim);

    // Left: the full roster in an inset well, scrolled, unknowns masked.
    ui::drawFrame(kListX - 12, kRowsY - 6, kListW + 22, kVisibleRows * kRowH + 10,
                  ui::FrameStyle::Inset);
    const int first = scroll_.top();
    const int count = scroll_.visibleCount(total, kVisibleRows);
    for (int row = 0; row < count; ++row) {
        const Entry& e = entries_[static_cast<std::size_t>(first + row)];
        const int y = kRowsY + row * kRowH;
        const bool sel = first + row == cursor_;
        if (sel) {
            ui::drawSelectionSlab(kListX - 9, y - 2, kListW + 15, kRowH);
            ui::drawChevron(kListX - 7, y + 1, style::palette().cursor, ui::motionPhase());
        }
        const Color c = e.known ? (sel ? style::palette().cursor : style::palette().text)
                                : style::palette().disabled;
        ui::drawTextFitted(e.known ? e.name : kUnknownName, kListX, y, kListW, style::kFontBody, c,
                           "bestiary.name");
    }

    // Right: the selected foe's detail panel.
    const int pw = w - kPanelX - 12;
    ui::drawFrame(kPanelX, 36, pw, h - 36 - style::kFooterHeight - 4, ui::FrameStyle::Standard);
    if (total > 0) {
        renderDetail(entries_[static_cast<std::size_t>(cursor_)], kPanelX, pw);
    }

    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Back"}},
                        w, h, "bestiary.footer");
}

}  // namespace cd
