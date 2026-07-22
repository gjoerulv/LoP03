#pragma once

#include "game/Achievements.hpp"

namespace cd {

class StateStack;
struct AppContext;

// M42: evaluates achievements against (party, ctx) via the global store, and for
// each NEWLY unlocked one pushes a dismissible "Achievement Unlocked!" toast (the
// existing dialog-overlay prompt pattern). Call from a state's onResume/completion
// path (never a constructor — queued pushes would sit under the pushing state).
void pushAchievementToasts(StateStack& stack, AppContext& context, const AchvContext& ctx);

}  // namespace cd
