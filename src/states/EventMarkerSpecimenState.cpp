#include "states/EventMarkerSpecimenState.hpp"

#ifdef CRYSTAL_CAPTURE

#include <string>

#include "content/ContentDatabase.hpp"
#include "core/AppContext.hpp"
#include "resource/ResourceManager.hpp"
#include "dungeon/ThemeEvents.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

EventMarkerSpecimenState::EventMarkerSpecimenState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void EventMarkerSpecimenState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void EventMarkerSpecimenState::render() {
    const int w = context_.virtualWidth;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Event Markers", w, p.crystal);

    // Two columns of eleven: the icon at the room's own size (1x, 12x12
    // inside a 16px cell), its flavor title fitted beside it. A missing icon
    // is a loud red block here - never a quiet fallback.
    constexpr int kRowH = 17;
    constexpr int kColW = 206;
    int i = 0;
    for (const dungeon::RoomEventKind kind : dungeon::kAllRoomEventKinds) {
        const int col = i / 11;
        const int row = i % 11;
        const int x = 10 + col * kColW;
        const int y = 32 + row * kRowH;
        const char* icon = dungeon::eventMarkerSpriteId(kind);
        if (icon != nullptr && context_.resources.hasTexture(icon)) {
            DrawTexture(context_.resources.texture(icon), x, y, WHITE);
        } else {
            DrawRectangle(x, y, 12, 12, Color{216, 90, 90, 255});
        }
        const content::EventFlavorDef* flavor =
            context_.content.findEventFlavor(dungeon::eventFlavorId(kind));
        const std::string title =
            flavor != nullptr ? flavor->title : std::string(dungeon::eventFlavorId(kind));
        ui::drawTextFitted(title, x + 18, y + 1, kColW - 24, ui::style::kFontBody, p.text,
                           "specimen.event.title");
        ++i;
    }
    ui::drawTextCentered("M118: one icon per event kind, at the size the room draws it",
                         w / 2, 224, ui::style::kFontSmall, p.textHint);
}

}  // namespace cd

#endif  // CRYSTAL_CAPTURE
