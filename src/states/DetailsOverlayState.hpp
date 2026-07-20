#pragma once

#include <string>

#include "states/GameState.hpp"

namespace cd {

struct AppContext;

// Contextual Details panel (M22): a transparent overlay opened by the
// Details action wherever a state offers it. Freezes and dims the scene
// below, shows a titled wrapped text block (paragraphs separated by '\n'),
// and Confirm/Cancel/Details closes it. Content is composed by the caller,
// which has the context — this state only presents.
class DetailsOverlayState : public GameState {
public:
    DetailsOverlayState(StateStack& stack, AppContext& context, std::string title,
                        std::string body);

    void handleInput(const Input& input) override;
    void render() override;

    bool rendersBelow() const override { return true; }

private:
    AppContext& context_;
    std::string title_;
    std::string body_;
};

}  // namespace cd
