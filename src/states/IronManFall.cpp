#include "states/IronManFall.hpp"

#include <memory>
#include <string>
#include <utility>

#include "content/LoadReport.hpp"
#include "core/AppContext.hpp"
#include "core/Log.hpp"
#include "game/FallenRuns.hpp"
#include "game/Party.hpp"
#include "save/SaveSystem.hpp"
#include "states/FallenState.hpp"
#include "states/StateStack.hpp"

namespace cd {

void beginIronManFall(StateStack& stack, AppContext& context, ironman::FallenInfo info) {
    // The record first - once, before anything is shown.
    FallenRun run;
    run.place = info.place;
    run.foes = info.foes;
    run.leader = context.party.members.empty() ? std::string{} : context.party.members[0].name;
    run.highestLevel = highestLevel(context.party);
    run.playSeconds = static_cast<long long>(context.party.lifetime.explore.playSeconds);
    run.party = context.saves.serialize(context.party);
    content::LoadReport report;
    if (!context.fallenRuns.record(std::move(run), report)) {
        // Best effort, like the achievements: the run still shows in the hall
        // for this sitting; only the file write failed.
        for (const auto& e : report.errors()) {
            log::warn("fallen runs: " + e.context + ": " + e.message);
        }
    }

    // Queued like every transition (the Quit-to-Title idiom): the caller's
    // frame finishes, then the whole run - dungeon, battle, town - is gone.
    stack.clearStates();
    stack.pushState(std::make_unique<FallenState>(stack, context, std::move(info)));
}

}  // namespace cd
