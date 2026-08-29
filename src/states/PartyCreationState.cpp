#include "states/PartyCreationState.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "content/ContentDatabase.hpp"
#include "content/Definitions.hpp"
#include "audio/AudioManager.hpp"
#include "core/AppContext.hpp"
#include "game/Cutscenes.hpp"  // M97: the new-game prologue trigger
#include "game/Profile.hpp"
#include "game/Party.hpp"
#include "input/Input.hpp"
#include "input/PromptLabels.hpp"
#include "raylib.h"
#include "resource/ResourceManager.hpp"  // 2026-08-29: the class-row sprites
#include "states/CutsceneState.hpp"
#include "states/DetailsOverlayState.hpp"  // owner request 2026-08-29: class sheets
#include "states/StateStack.hpp"
#include "states/TownState.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {
constexpr int kBeginRow = 4;
constexpr int kRows = 5;
const char* const kDefaultNames[4] = {"Rolan", "Mira", "Sable", "Pell"};
}  // namespace

PartyCreationState::PartyCreationState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void PartyCreationState::onEnter() {
    // Canonical class order first, then any extras the data may add.
    static const char* const order[] = {"knight", "ranger", "mage", "cleric", "rogue", "guardian"};
    classes_.clear();
    for (const char* id : order) {
        if (const content::ClassDef* c = context_.content.findClass(id)) {
            classes_.push_back(c);
        }
    }
    for (const auto& [id, def] : context_.content.classes()) {
        if (std::find(classes_.begin(), classes_.end(), &def) == classes_.end()) {
            classes_.push_back(&def);
        }
    }

    ready_ = !classes_.empty();
    for (int i = 0; i < 4; ++i) {
        slots_[static_cast<std::size_t>(i)].classIndex =
            ready_ ? (i % static_cast<int>(classes_.size())) : 0;
        slots_[static_cast<std::size_t>(i)].name = ui::TextInput(12, kDefaultNames[i]);
    }
    cursor_ = 0;
    editing_ = false;
#ifdef CRYSTAL_CAPTURE
    for (std::size_t slot = 0; slot < capturePending_.size(); ++slot) {
        if (capturePending_[slot].empty()) {
            continue;
        }
        for (std::size_t i = 0; i < classes_.size(); ++i) {
            if (classes_[i]->id == capturePending_[slot]) {
                slots_[slot].classIndex = static_cast<int>(i);
                break;
            }
        }
    }
#endif
}

bool PartyCreationState::classLocked(const content::ClassDef& cls) const {
    // M45: the King's reward classes stay visible but unusable until he falls —
    // the roster's size and the goal are never hidden from the player.
    return cls.unlockedByKing && !context_.profile.classesUnlocked();
}

#ifdef CRYSTAL_CAPTURE
void PartyCreationState::captureSelectClass(int slot, const std::string& classId) {
    if (slot >= 0 && slot < 4) {
        capturePending_[static_cast<std::size_t>(slot)] = classId;
    }
}
#endif

void PartyCreationState::cycleClass(int slotIndex, int direction) {
    if (classes_.empty()) {
        return;
    }
    const int n = static_cast<int>(classes_.size());
    Slot& slot = slots_[static_cast<std::size_t>(slotIndex)];
    slot.classIndex = ((slot.classIndex + direction) % n + n) % n;
}

bool PartyCreationState::anyLockedSelected() const {
    for (const Slot& slot : slots_) {
        if (classLocked(*classes_[static_cast<std::size_t>(slot.classIndex)])) {
            return true;
        }
    }
    return false;
}

void PartyCreationState::begin() {
    // A New Game is a clean slate — the WHOLE Party object (owner bug report
    // 2026-08-29: the old hand-picked clear list leaked every field added
    // after it, so a loaded save's Guild unlock survived into a fresh game).
    // See game/Party.hpp resetForNewGame for the contract.
    resetForNewGame(context_.party);
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        std::string name = slots_[i].name.trimmed();
        if (name.empty()) {
            name = kDefaultNames[i];
        }
        const content::ClassDef* cls = classes_[static_cast<std::size_t>(slots_[i].classIndex)];
        context_.party.members.push_back(createCharacter(*cls, name));
    }

    // Starter supplies so the Item command and revives are usable from the start.
    context_.party.inventory.add("potion", 3);
    context_.party.inventory.add("antidote", 1);
    context_.party.inventory.add("ether", 1);
    context_.party.inventory.add("phoenix_tear", 1);

    stack().clearStates();
    stack().pushState(std::make_unique<TownState>(stack(), context_));
    // M97: the prologue — the hooded stranger meets the new party before the
    // first town breathes. Marked seen before the push (the storyMet idiom);
    // an unauthored scene simply never plays.
    if (context_.content.findCutscene("new_game") != nullptr &&
        !game::cutsceneSeen(context_.party, "new_game")) {
        game::markCutsceneSeen(context_.party, "new_game");
        stack().pushState(
            std::make_unique<CutsceneState>(stack(), context_, "new_game", /*replay=*/false));
    }
}

void PartyCreationState::handleInput(const Input& input) {
    if (!ready_) {
        if (input.pressed(InputAction::Cancel)) {
            stack().popState();
        }
        return;
    }

    if (editing_) {
        // Everything flows through the input layer (M13; fixes the
        // gamepad-only soft-lock UI-INPUT-001): typed characters via
        // nextChar, delete via TextBackspace (Backspace / gamepad X), and
        // Confirm or Cancel — from either device — finishes editing.
        // TextBackspace wins over Cancel when both fire from the same
        // Backspace press.
        for (int c = input.nextChar(); c > 0; c = input.nextChar()) {
            slots_[static_cast<std::size_t>(cursor_)].name.appendCodepoint(c);
        }
        if (input.navPressed(InputAction::TextBackspace)) {
            slots_[static_cast<std::size_t>(cursor_)].name.backspace();
        } else if (input.pressed(InputAction::Confirm) || input.pressed(InputAction::Cancel)) {
            editing_ = false;
        }
        return;
    }

    if (input.navPressed(InputAction::MoveUp)) {
        cursor_ = (cursor_ + kRows - 1) % kRows;
    }
    if (input.navPressed(InputAction::MoveDown)) {
        cursor_ = (cursor_ + 1) % kRows;
    }
    if (cursor_ < kBeginRow) {
        if (input.navPressed(InputAction::MoveLeft)) {
            cycleClass(cursor_, -1);
        }
        if (input.navPressed(InputAction::MoveRight)) {
            cycleClass(cursor_, +1);
        }
    }
    // Owner request 2026-08-29: the Details key opens the highlighted class's
    // full sheet (locked classes too — the goal is never hidden).
    if (cursor_ < kBeginRow && input.pressed(InputAction::Details)) {
        openClassDetails();
        return;
    }
    if (input.pressed(InputAction::Confirm)) {
        if (cursor_ < kBeginRow) {
            editing_ = true;
        } else if (anyLockedSelected()) {
            context_.audio.play(Sfx::Error);  // M45: a locked class cannot start a run
        } else {
            begin();
        }
    }
    if (input.pressed(InputAction::Cancel)) {
        stack().popState();
    }
}

void PartyCreationState::openClassDetails() {
    const content::ClassDef& cls =
        *classes_[static_cast<std::size_t>(slots_[static_cast<std::size_t>(cursor_)].classIndex)];
    std::string body = cls.role;
    body += TextFormat("\n\nBase:  HP %d   ATK %d   MAG %d   DEF %d   SPD %d",
                       cls.baseStats.maxHp, cls.baseStats.attack, cls.baseStats.magic,
                       cls.baseStats.defense, cls.baseStats.speed);
    body += TextFormat("\nPer level:  +%.1f HP   +%.1f ATK   +%.1f MAG   +%.1f DEF   +%.1f SPD",
                       cls.growth.maxHp, cls.growth.attack, cls.growth.magic,
                       cls.growth.defense, cls.growth.speed);
    const auto skillName = [&](const std::string& id) {
        const content::SkillDef* s = context_.content.findSkill(id);
        return s != nullptr ? s->name : id;
    };
    if (!cls.startingSkills.empty()) {
        body += "\nStarts with:";
        for (const std::string& sid : cls.startingSkills) {
            body += " " + skillName(sid) + ",";
        }
        body.pop_back();
    }
    if (!cls.learnset.empty()) {
        body += "\nLearns:";
        for (const content::LearnEntry& e : cls.learnset) {
            body += " " + skillName(e.skill) + TextFormat(" (Lv %d),", e.level);
        }
        body.pop_back();
    }
    // The M45 quirks, stated outright — never color or lore alone.
    for (content::EquipSlot banned : cls.equipBans) {
        switch (banned) {
            case content::EquipSlot::Weapon: body += "\nHolds no weapon."; break;
            case content::EquipSlot::Armor: body += "\nWears no armor."; break;
            case content::EquipSlot::Accessory: body += "\nWears no accessory."; break;
            case content::EquipSlot::Heirloom: body += "\nCarries no heirloom."; break;
            case content::EquipSlot::None: break;
        }
    }
    if (cls.attackHitsAll) {
        body += "\nIts basic attack strikes every foe.";
    }
    if (cls.uncontrolled) {
        body += "\nActs entirely on its own - it never takes commands.";
    }
    if (cls.scoreModPct != 0) {
        body += TextFormat("\nScore modifier: %+d%% per member in the party.", cls.scoreModPct);
    }
    if (classLocked(cls)) {
        body += "\n\nLocked: defeat the Hollow King to unlock this class.";
    }
    context_.audio.play(Sfx::Confirm);
    stack().pushState(
        std::make_unique<DetailsOverlayState>(stack(), context_, cls.name, std::move(body)));
}

void PartyCreationState::update(float dt) { caretTimer_ += dt; }

void PartyCreationState::render() {
    const int w = context_.virtualWidth;
    const int h = context_.virtualHeight;
    namespace style = ui::style;
    const style::Palette& p = style::palette();
    ClearBackground(p.canvas);
    ui::drawTitlePlaque("Create Your Party", w / 2, 8, 16);

    if (!ready_) {
        ui::drawTextCentered("No classes loaded. Press Cancel to go back.", w / 2, h / 2, 12,
                             p.dangerText);
        return;
    }

    // Roster panel: class sprite, name column, and a class stepper capsule
    // per member (the sprite joined 2026-08-29 by owner request — cycling a
    // class shows its look at once; locked classes grey to a silhouette).
    ui::drawFrame(24, 46, w - 48, 4 * 24 + 14, ui::FrameStyle::Standard);
    const int baseY = 56;
    for (int i = 0; i < 4; ++i) {
        const int y = baseY + i * 24;
        const bool selected = cursor_ == i;

        const content::ClassDef* cls =
            classes_[static_cast<std::size_t>(slots_[static_cast<std::size_t>(i)].classIndex)];
        const bool locked = classLocked(*cls);

        const std::string spriteId = "actor." + cls->id + ".battle";
        if (context_.resources.hasTexture(spriteId)) {
            const Texture2D& tex = context_.resources.texture(spriteId);
            DrawTextureEx(tex, Vector2{30.0f, static_cast<float>(y - 6)}, 0.0f, 1.0f,
                          locked ? p.disabled : WHITE);
        }

        if (selected) {
            ui::drawSelectionSlab(56, y - 3, 112, 17);
            ui::drawChevron(59, y + 2, p.cursor, ui::motionPhase());
        }
        std::string nameText = slots_[static_cast<std::size_t>(i)].name.value();
        if (editing_ && selected) {
            nameText += (std::fmod(caretTimer_, 1.0f) < 0.5f) ? "_" : " ";
        }
        ui::drawText(nameText.c_str(), 70, y, 12, selected ? p.cursor : p.text);
        const Color classColor = locked ? p.disabled : (selected ? p.cursor : p.text);
        const int capW = 156;
        const int capX = w - 60 - capW;
        ui::drawStepArrow(capX - 11, y + 2, -1, selected);
        ui::drawStepArrow(capX + capW + 6, y + 2, +1, selected);
        DrawRectangle(capX, y - 2, capW, 15, p.ink);
        DrawRectangle(capX + 1, y - 1, capW - 2, 13, p.chipFill);
        if (selected) {
            DrawRectangleLines(capX, y - 2, capW, 15, p.rowBorder);
        }
        const std::string classLabel =
            std::string(cls->name) + (locked ? " (Locked)" : "");
        const int cw = ui::measureText(classLabel, 10);
        ui::drawTextFitted(classLabel, capX + (capW - cw) / 2, y + 1, capW - 6, 10, classColor,
                           "partycreate.class");
    }

    // "Begin Adventure" — the dominant call to action, Guild-style.
    const int beginY = 172;
    const bool beginSel = cursor_ == kBeginRow;
    ui::drawFrame(w / 2 - 92, beginY - 5, 184, 22,
                  beginSel ? ui::FrameStyle::Reward : ui::FrameStyle::Raised);
    ui::drawTextCentered("Begin Adventure", w / 2, beginY, 12, beginSel ? p.cursor : p.text);
    if (beginSel) {
        ui::drawChevron(w / 2 - 84, beginY + 2, p.cursor, ui::motionPhase());
    }

    // M45: say WHY a class is locked, once, under the roster (never color alone).
    if (anyLockedSelected()) {
        const char* note = "Locked: defeat the Hollow King to unlock these classes.";
        const int noteW = ui::measureText(note, 10);
        const int noteX = w / 2 - noteW / 2 + 6;
        DrawRectangle(noteX - 13, beginY + 24, 2, 2, p.danger);
        DrawRectangle(noteX - 15, beginY + 26, 6, 2, p.danger);
        DrawRectangle(noteX - 17, beginY + 28, 10, 2, p.danger);
        ui::drawTextFitted(note, noteX, beginY + 22, w - noteX - 8, 10, p.dangerText,
                           "partycreate.locked");
    }

    const InputMap& map = context_.input.map();
    const ActiveDevice device = context_.input.activeDevice();
    if (editing_) {
        const std::string editHint =
            device == ActiveDevice::Keyboard
                ? "Type a name   " +
                      input::prompt(map, InputAction::TextBackspace, device, "Delete") +
                      "   " + input::prompt(map, InputAction::Confirm, device, "Finish")
                : input::prompt(map, InputAction::TextBackspace, device, "Delete") + "   " +
                      input::prompt(map, InputAction::Confirm, device, "Finish") +
                      "   (typing needs a keyboard)";
        ui::drawTextCentered(editHint.c_str(), w / 2, h - 28, 10, p.success);
    }
    ui::drawFooterHints(
        {{input::primaryLabel(map, InputAction::MoveUp, device) + "/" +
              input::primaryLabel(map, InputAction::MoveDown, device),
          "Move"},
         {input::primaryLabel(map, InputAction::MoveLeft, device) + "/" +
              input::primaryLabel(map, InputAction::MoveRight, device),
          "Class"},
         {input::primaryLabel(map, InputAction::Confirm, device), "Edit/Begin"},
         // "Info", not "Class Info": five hints must share the 418px strip
         // (the capture lint measures exactly this line).
         {input::primaryLabel(map, InputAction::Details, device), "Info"},
         {input::primaryLabel(map, InputAction::Cancel, device), "Back"}},
        w, h, "partycreate.footer");
}

}  // namespace cd
