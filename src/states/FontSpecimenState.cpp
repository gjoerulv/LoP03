#include "states/FontSpecimenState.hpp"

#ifdef CRYSTAL_CAPTURE

#include <string>

#include "core/AppContext.hpp"
#include "input/Input.hpp"
#include "raylib.h"
#include "states/StateStack.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd {

namespace {

// The M87 Latin pangram (the same bytes CaptureRunner's pseudo-localization
// scenes use), kept here so the specimen stands on its own.
const char* kSpecimenPangram =
    "\xC3\x86" "blegr\xC3\xB8" "d p\xC3\xA5 \xC3\x98" "rken\xC3\xB8" "y: "
    "\xC3\xA4\xC3\xB6\xC3\xBC \xC3\x84\xC3\x96\xC3\x9C \xC3\x9F, "
    "\xC3\xA7 \xC3\xA9\xC3\xA8\xC3\xAA\xC3\xAB \xC3\xA0\xC3\xA2 "
    "\xC3\xAD\xC3\xAC\xC3\xAE\xC3\xAF \xC3\xB3\xC3\xB2\xC3\xB4\xC3\xB5 "
    "\xC3\xBA\xC3\xB9\xC3\xBB \xC3\xB1 \xC3\xBD\xC3\xBF \xC3\xB0\xC3\xBE "
    "\xC2\xA1hola! \xC2\xBF" "qu\xC3\xA9? \xC2\xAB" "cit\xC3\xA9\xC2\xBB";

}  // namespace

FontSpecimenState::FontSpecimenState(StateStack& stack, AppContext& context)
    : GameState(stack), context_(context) {}

void FontSpecimenState::handleInput(const Input& input) {
    if (input.pressed(InputAction::Cancel) || input.pressed(InputAction::Confirm)) {
        stack().popState();
    }
}

void FontSpecimenState::render() {
    const int w = context_.virtualWidth;
    const ui::style::Palette& p = ui::style::palette();
    ClearBackground(p.canvas);
    ui::drawHeaderBand("Font Specimen", w, p.cursor);

    // Every line goes through a fitted draw so the overflow lint referees
    // the widths at each exact size.
    int y = 30;
    const int x = 8;
    const int maxW = w - 16;
    const auto line = [&](const std::string& text, int size, Color color, const char* site) {
        ui::drawTextFitted(text, x, y, maxW, size, color, site);
        y += size + 4;
    };
    line("Il1 O0 CG ce rnm uv  \"paired\" 'quotes'  ;:,.!?  (){}[]<>  @#$%&*+-=/\\^_`~",
         ui::style::kFontSmall, p.text, "specimen.confusables.small");
    line("Il1 O0 CG ce rnm uv  \"paired\" 'quotes'  ;:,.!?  (){}[]<>", ui::style::kFontBody, p.text,
         "specimen.confusables.body");
    line("Il1 O0 CG ce rnm uv  \"paired\" 'quotes'", 20, p.text, "specimen.confusables.title");
    y += 2;
    line("The quick brown fox jumps over the lazy dog. 0123456789", ui::style::kFontSmall, p.text,
         "specimen.pangram.small");
    line("THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG. 0123456789", ui::style::kFontSmall, p.textDim,
         "specimen.pangram.small.caps");
    line("The quick brown fox jumps over the lazy dog. 0123456789", ui::style::kFontBody, p.text,
         "specimen.pangram.body");
    line("Quick brown fox, lazy dog 0123456789", 20, p.gold, "specimen.pangram.title");
    y += 2;
    line(kSpecimenPangram, ui::style::kFontSmall, p.text, "specimen.latin.small");
    line(kSpecimenPangram, ui::style::kFontBody, p.text, "specimen.latin.body");
    line("\xC3\x80\xC3\x81\xC3\x82\xC3\x83\xC3\x84\xC3\x85\xC3\x86\xC3\x87\xC3\x88\xC3\x89"
         "\xC3\x8A\xC3\x8B\xC3\x8C\xC3\x8D\xC3\x8E\xC3\x8F\xC3\x90\xC3\x91\xC3\x92\xC3\x93"
         "\xC3\x94\xC3\x95\xC3\x96\xC3\x98\xC3\x99\xC3\x9A\xC3\x9B\xC3\x9C\xC3\x9D\xC3\x9E"
         "\xC3\x9F",
         ui::style::kFontBody, p.textDim, "specimen.latin.caps");
    line("HP 136/136  MP 26/26  Turns 0  Patrol 100  KO  RFL PSN BLD  [Enter] Begin  Lv.12",
         ui::style::kFontSmall, p.textHint, "specimen.hud");
}

}  // namespace cd

#endif  // CRYSTAL_CAPTURE
