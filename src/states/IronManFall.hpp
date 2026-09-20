#pragma once

#include "game/IronMan.hpp"

namespace cd {

struct AppContext;
class StateStack;

// M123 - the end of an Iron Man run.
//
// `beginIronManFall` is the ONE door every real wipe leaves through (the
// dungeon's Defeat branch, a lost castle challenge, a lost treasure dig -
// never the sparring mirror, and never an escape). There is nothing to
// reload - SaveSystem never wrote this party.
//
// M124: the door first RECORDS the run in the Hall of Shame (where, who, and
// a snapshot of the party through the slot codec - once, before anything is
// shown, so quitting during the send-off cannot lose it), then drops the
// whole stack for the send-off: FallenState (the geese, the ducks, the King)
// -> the fallen run's summary -> the title. M123's plain notice is gone.
void beginIronManFall(StateStack& stack, AppContext& context, ironman::FallenInfo info);

}  // namespace cd
