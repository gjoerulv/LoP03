#include "states/MapsState.hpp"

#include <algorithm>
#include <string>

#include "audio/AudioManager.hpp"
#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "game/Curios.hpp"  // M66
#include "game/Party.hpp"
#include "game/TreasureMap.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

// One quadrant of the treasure sketch (all original, drawn from primitives;
// deterministic — no RNG). qx/qy are the quadrant's top-left; each quadrant
// is 100x56 inside the 200x112 parchment.
void drawQuadrant(int q, int qx, int qy, const ui::style::Palette& p) {
    const Color ink = p.textDim;
    const Color accent = p.gold;
    switch (q) {
        case 0:  // TL: a ragged coastline and two gulls
            for (int i = 0; i < 5; ++i) {
                DrawRectangle(qx + 8 + i * 18, qy + 34 + (i % 2) * 4, 14, 2, ink);
            }
            DrawRectangle(qx + 22, qy + 10, 5, 2, ink);
            DrawRectangle(qx + 25, qy + 8, 5, 2, ink);
            DrawRectangle(qx + 60, qy + 16, 5, 2, ink);
            DrawRectangle(qx + 63, qy + 14, 5, 2, ink);
            break;
        case 1:  // TR: a mountain ridge with a snowcap
            for (int i = 0; i < 3; ++i) {
                const int mx = qx + 14 + i * 28;
                const int mh = 18 + (i == 1 ? 10 : 0);
                for (int s = 0; s < mh; s += 2) {
                    DrawRectangle(mx + s / 2, qy + 44 - s, mh - s, 2, ink);
                }
                if (i == 1) {
                    DrawRectangle(mx + mh / 2 - 3, qy + 44 - mh, 6, 3, p.text);
                }
            }
            break;
        case 2:  // BL: a scatter of forest crowns
            for (int i = 0; i < 6; ++i) {
                const int tx = qx + 12 + (i % 3) * 30;
                const int ty = qy + 10 + (i / 3) * 22;
                DrawRectangle(tx + 2, ty, 8, 6, ink);
                DrawRectangle(tx, ty + 4, 12, 6, ink);
                DrawRectangle(tx + 5, ty + 10, 2, 5, ink);
            }
            break;
        case 3:  // BR: the dotted path to a bold X
            for (int i = 0; i < 7; ++i) {
                DrawRectangle(qx + 6 + i * 10, qy + 8 + i * 5, 3, 3, ink);
            }
            for (int s = 0; s < 12; ++s) {
                DrawRectangle(qx + 72 + s, qy + 36 + s, 2, 2, accent);
                DrawRectangle(qx + 83 - s, qy + 36 + s, 2, 2, accent);
            }
            break;
        default:
            break;
    }
}

}  // namespace

MapsState::MapsState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

#ifdef CRYSTAL_CAPTURE
void MapsState::captureInspect(int index) {
    cursor_ = index < 0 ? 0 : (index >= kCurioCount ? kCurioCount - 1 : index);
    loreOpen_ = true;
}
#endif

void MapsState::handleInput(const Input& input) {
    // M85: the lore panel is a modal — any confirm/cancel closes it.
    // M87: Up/Down scrolls a long body first.
    if (loreOpen_) {
        if (input.navPressed(InputAction::MoveUp) && loreView_.scrollBy(-1)) {
            context_.audio.play(Sfx::Move);
        }
        if (input.navPressed(InputAction::MoveDown) && loreView_.scrollBy(1)) {
            context_.audio.play(Sfx::Move);
        }
        if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
            context_.audio.play(Sfx::Cancel);
            loreOpen_ = false;
        }
        return;
    }
    // M85: the curio grid is navigable (4 columns x 3 rows, clamped edges).
    const int col = cursor_ % 4;
    const int row = cursor_ / 4;
    if (input.navPressed(InputAction::MoveLeft) && col > 0) {
        --cursor_;
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveRight) && col < 3) {
        ++cursor_;
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveUp) && row > 0) {
        cursor_ -= 4;
        context_.audio.play(Sfx::Move);
    }
    if (input.navPressed(InputAction::MoveDown) && row < 2) {
        cursor_ += 4;
        context_.audio.play(Sfx::Move);
    }
    if (input.pressed(InputAction::Confirm)) {
        // Inspect an OWNED curio; an unfound one keeps its secret.
        if (ownsCurio(context_.party.ownedCurios,
                      kCurios[static_cast<std::size_t>(cursor_)].id)) {
            context_.audio.play(Sfx::Confirm);
            loreOpen_ = true;
        } else {
            context_.audio.play(Sfx::Error);
        }
        return;
    }
    if (input.pressed(InputAction::Cancel)) {
        context_.audio.play(Sfx::Cancel);
        stack().popState();
    }
}

void MapsState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Maps & Treasures", w, p.gold);

    const Party& party = context_.party;
    const bool revealed = party.treasure.active;
    const int shown = revealed ? kMapPiecesNeeded : party.mapPieces;

    // The parchment: 2x2 quadrants, hidden ones written over with "?".
    const int mapW = 200;
    const int mapH = 112;
    const int mapX = w / 2 - mapW / 2;
    const int mapY = 36;
    ui::drawFrame(mapX - 8, mapY - 8, mapW + 16, mapH + 16, ui::FrameStyle::Reward);
    DrawRectangle(mapX, mapY, mapW, mapH, ui::lighten(p.canvas, 14));
    // Quadrants reveal in a fixed order: TL, TR, BL, BR — HoMM2 style.
    for (int q = 0; q < 4; ++q) {
        const int qx = mapX + (q % 2) * (mapW / 2);
        const int qy = mapY + (q / 2) * (mapH / 2);
        if (q < shown) {
            drawQuadrant(q, qx, qy, p);
        } else {
            DrawRectangle(qx + 2, qy + 2, mapW / 2 - 4, mapH / 2 - 4, p.canvas);
            ui::drawTextCentered("?", qx + mapW / 4, qy + mapH / 4 - 5, 14, p.textHint);
        }
    }
    DrawRectangle(mapX, mapY + mapH / 2 - 1, mapW, 1, p.rowBorder);
    DrawRectangle(mapX + mapW / 2 - 1, mapY, 1, mapH, p.rowBorder);

    std::string line;
    if (revealed) {
        const content::BossDef* guard = context_.content.findBoss(party.treasure.bossId);
        line = TextFormat("The treasure lies buried in TOWN %d - find the dig spot, but beware: %s",
                          party.treasure.town,
                          guard != nullptr ? ("it is guarded by " + guard->name + ".").c_str()
                                           : "it is guarded.");
    } else if (shown == 0) {
        line = TextFormat(
            "Secret Map Pieces hide in some dungeons. Gather four. Lost Scrolls found: %d of %d.",
            static_cast<int>(party.treasureScrollsAwarded.size()),
            static_cast<int>(treasureScrollPool().size()));
    } else {
        line = TextFormat("%d of %d pieces gathered. The sketch takes shape...", shown,
                          kMapPiecesNeeded);
    }
    // M83: the guild's IOU bank — pieces earned from 4-floor descents while
    // the treasure stood revealed, paid out after the dig.
    if (party.mapPiecesOwed > 0) {
        line += TextFormat("  The guild owes %d piece%s, payable after the dig.",
                           party.mapPiecesOwed, party.mapPiecesOwed == 1 ? "" : "s");
    }
    ui::drawTextWrapped(line, 40, mapY + mapH + 10, w - 80, ui::style::kFontSmall, revealed ? p.gold : p.textDim,
                        "maps.status", 2);

    // M66: the curio collection — twelve trinkets paid out by the single-use
    // dungeon treasure maps, four columns by three rows, locked ones masked.
    const int curioY = mapY + mapH + 30;
    ui::drawSectionHeader(TextFormat("Curios  %d / %d",
                                     static_cast<int>(party.ownedCurios.size()), kCurioCount),
                          40, curioY, w - 80);
    const int colW = (w - 60) / 4;
    for (int i = 0; i < kCurioCount; ++i) {
        const CurioDef& cd = kCurios[i];
        const bool owned = ownsCurio(party.ownedCurios, cd.id);
        const int cxp = 30 + (i % 4) * colW;
        const int cyp = curioY + 12 + (i / 4) * 10;
        // M85: the grid cursor — a slab under the focused name, gold when it
        // can be inspected.
        if (i == cursor_ && !loreOpen_) {
            ui::drawSelectionSlab(cxp - 3, cyp - 1, colW - 2, 11);
        }
        ui::drawTextFitted(owned ? cd.name : "? ? ?", cxp, cyp, colW - 6, ui::style::kFontSmall,
                           i == cursor_ ? (owned ? p.gold : p.textDim)
                                        : (owned ? p.text : p.textHint),
                           "maps.curio");
    }

    // M85: the lore panel — the curio speaks (curio_lore.json), or falls back
    // to its own M66 description when the optional file is absent. M87: the
    // body is a bounded scrollable viewport (up to 7 visible lines).
    if (loreOpen_) {
        const CurioDef& cd = kCurios[static_cast<std::size_t>(cursor_)];
        const content::CurioLoreDef* lore = context_.content.findCurioLore(cd.id);
        const std::string body = lore != nullptr ? lore->body : std::string(cd.description);
        ui::drawModalDim(w, h);
        const int boxW = 340;
        const int boxH = 130;
        const int boxX = w / 2 - boxW / 2;
        const int boxY = h / 2 - boxH / 2;
        ui::drawFrame(boxX, boxY, boxW, boxH, ui::FrameStyle::Reward);
        ui::drawTextCentered(cd.name, w / 2, boxY + 10, 14, p.gold);
        ui::drawDivider(boxX + 14, boxY + 28, boxW - 28);
        loreView_.setContent(body, boxW - 32 - ui::kScrollGutterW, ui::style::kFontSmall, ui::raylibMeasure());
        loreView_.setVisibleLines(std::clamp(loreView_.lineCount(), 1, 7));
        ui::drawTextViewport(loreView_, boxX + 16, boxY + 36, p.text);
        std::string hint = input::prompt(context_.input.map(), InputAction::Confirm,
                                         context_.input.activeDevice(), "Close");
        if (loreView_.scrollable()) {
            hint = input::primaryLabel(context_.input.map(), InputAction::MoveUp,
                                       context_.input.activeDevice()) +
                   "/" +
                   input::primaryLabel(context_.input.map(), InputAction::MoveDown,
                                       context_.input.activeDevice()) +
                   " Scroll   " + hint;
        }
        ui::drawTextCentered(hint.c_str(), w / 2, boxY + boxH - 14, ui::style::kFontSmall, p.textDim);
    }

    ui::drawFooterHints({{input::primaryLabel(context_.input.map(), InputAction::Confirm,
                                              context_.input.activeDevice()),
                          "Inspect"},
                         {input::primaryLabel(context_.input.map(), InputAction::Cancel,
                                              context_.input.activeDevice()),
                          "Back"}},
                        w, h, "maps.footer");
}

}  // namespace cd
