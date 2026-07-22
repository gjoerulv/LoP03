#include "states/AchievementToast.hpp"

#include <memory>
#include <string>
#include <vector>

#include "core/AppContext.hpp"
#include "game/Achievements.hpp"
#include "states/StateStack.hpp"
#include "states/StoryDialogState.hpp"

namespace cd {

void pushAchievementToasts(StateStack& stack, AppContext& context, const AchvContext& ctx) {
    const std::vector<std::string> newly = context.achievements.check(context.party, ctx);
    for (const std::string& id : newly) {
        if (const AchievementDef* a = findAchievement(id)) {
            stack.pushState(std::make_unique<StoryDialogState>(
                stack, context, "Achievement Unlocked!", a->name, a->description));
        }
    }
}

}  // namespace cd
