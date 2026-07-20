#pragma once

// Deterministic native-resolution capture mode (M23, owner-approved):
// `CrystalDungeons --capture <outdir>` renders a fixed scenario list to the
// real 426x240 virtual screen, writes one PNG per scene, and fails (nonzero
// exit) if any scene produced a text-overflow diagnostic. Compiled only when
// CRYSTAL_CAPTURE is defined (dev builds; Release excludes it). All mutable
// state (saves/settings/scoreboard/tutorial) lives in a scratch directory —
// the player's real data is never touched.

#ifdef CRYSTAL_CAPTURE

namespace cd::capture {

// Returns a process exit code: 0 = all scenes captured with zero overflow.
int run(const char* outDir);

}  // namespace cd::capture

#endif  // CRYSTAL_CAPTURE
