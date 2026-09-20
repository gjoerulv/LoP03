#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/TextViewport.hpp"

namespace cd {

struct AppContext;

// Contextual Details panel (M22; scrollable since M87): a transparent overlay
// opened by the Details action wherever a state offers it. Freezes and dims
// the scene below and shows a titled prose body (paragraphs separated by
// '\n') in a bounded, scrollable viewport — the panel caps inside the safe
// area and Up/Down reaches every remaining line, so a body of any length
// (e.g. a translated one) is fully readable. Confirm/Cancel/Details closes.
// Content is composed by the caller, which has the context — this state only
// presents. This is the canonical reading overlay other screens route long
// text through.
class DetailsOverlayState : public GameState {
public:
    DetailsOverlayState(StateStack& stack, AppContext& context, std::string title,
                        std::string body);
    // M121: a titled sheet with up to two 10x10 icons flanking the title (a
    // skill's kind icon on the left, the milestone mark on the right). Either
    // id may be empty; a missing texture draws nothing (placeholder rule).
    DetailsOverlayState(StateStack& stack, AppContext& context, std::string title,
                        std::string body, std::string leftIconId, std::string rightIconId);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }

#ifdef CRYSTAL_CAPTURE
    // Capture-only: scroll the body by the given lines (after one render has
    // sized the viewport) so the clamped-bottom rendering is lint-checked.
    void captureScroll(int deltaLines) { pendingCaptureScroll_ = deltaLines; }
#endif

private:
    AppContext& context_;
    std::string title_;
    std::string body_;
    std::string leftIconId_;   // M121 (empty = none)
    std::string rightIconId_;  // M121 (empty = none)
    ui::TextViewport bodyView_;
#ifdef CRYSTAL_CAPTURE
    int pendingCaptureScroll_ = 0;
#endif
};

}  // namespace cd
