#pragma once

#include <string>

#include "states/GameState.hpp"
#include "ui/TextViewport.hpp"

namespace cd {

struct AppContext;

// M120: the title screen's Credits & Licenses page (it took the Controls
// row's place; the Controls page moved under Settings -> Controls). One credit
// line, then the third-party license texts in a scrollable viewport (policy
// C). The text is the SAME file the package ships — packaging/LICENSES.txt,
// embedded at configure time as core/Licenses.hpp and reflowed for the
// viewport by the pure game/Credits.hpp. Presentation only: reads nothing at
// runtime, writes nothing.
class CreditsState : public GameState {
public:
    CreditsState(StateStack& stack, AppContext& context);

    void handleInput(const Input& input) override;
    bool pausesPlayClock() const override { return true; }  // a menu, not play
    void render() override;

#ifdef CRYSTAL_CAPTURE
    // Capture-only: park the viewport `lines` down so the overflow lint also
    // sees the license prose, not just the opening paragraph.
    void captureScroll(int lines) { pendingCaptureScroll_ = lines; }
#endif

private:
    AppContext& context_;
    std::string body_;
    ui::TextViewport bodyView_;
#ifdef CRYSTAL_CAPTURE
    int pendingCaptureScroll_ = 0;
#endif
};

}  // namespace cd
