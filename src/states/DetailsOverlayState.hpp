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
    ui::TextViewport bodyView_;
#ifdef CRYSTAL_CAPTURE
    int pendingCaptureScroll_ = 0;
#endif
};

}  // namespace cd
