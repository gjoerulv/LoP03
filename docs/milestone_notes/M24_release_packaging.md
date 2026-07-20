# M24 — Release Packaging and Final Release Validation

## A. Status and authority

- **Status:** in progress (packaging engineering; final validation gated
  on M23 per the owner's parallel-flow decision, 2026-07-20)
- **Last reviewed repository commit:** M22/M23-interim commit HEAD
  (2026-07-20). Re-audit: no `CMakePresets.json`; no version metadata,
  icon, or packaging script; the build defaults to the **DLL runtime**
  (`/MD`) — a clean machine without the VC++ redistributable would not
  launch the exe (fixed via static runtime in the Release preset); logs go
  through raylib TraceLog to stdout (invisible in the WIN32 Release app);
  `resolveBundledDir` finds `data/`/`assets/` beside the exe, so the
  staged layout needs no path work; user data stays in
  `paths::userDataDir()` regardless of install location.
- **Owner decisions (2026-07-20):**
  1. **Version 0.9.0** for the release candidate; the bump to 1.0.0
     happens only after the M23 playtests complete and the owner approves.
  2. **Plain zip folder** distribution
     (`CrystalDungeons-<version>-win64.zip`); no installer.
  3. **Icon generated from the approved crystal emblem** by the
     in-project deterministic generator.
  4. (Flow) M24 engineering runs in parallel with the open M23; the final
     manual-matrix sign-off and release-candidate acceptance wait for
     M23's completion.
  5. The owner reviewed and approved the interim packaging engineering
     (2026-07-20); this does not advance the milestone status — final
     acceptance still requires the §J validation on the packaged build.
- **Relationship to `docs/milestones.md`:** single authoritative detailed scope
  for the M24 ledger entry; the ledger holds status. On conflict, follow the
  authority order in `CLAUDE.md`.
- **Boundary note:** playtesting and balance hardening moved to M23; this
  final milestone is pure packaging and validation, deliberately low-risk.
- **Planning fidelity:** bounded specification; refresh this note against the
  then-current repository before implementation begins.

## B. Problem statement

The M10 "release" is a developer deliverable: `CrystalDungeons.exe` plus
`data/` from a manual MSVC build, no presets (no `CMakePresets.json` exists at
the reviewed commit), no version metadata or icon, no packaged `assets/`
tree (which will exist after M14–M21), no consolidated credits/licenses, and
no evidence the build runs on a machine without Visual Studio. The polished
game from M11–M23 needs a reproducible, clean, validated distribution.

## C. Goals

- A reproducible Release build path — a committed `CMakePresets.json` (or
  equivalently documented preset) producing the Release configuration
  deterministically.
- A packaged distribution layout: executable + `data/` + `assets/` +
  credits/licenses + README-for-players, with stable save/settings paths.
- Executable version metadata and icon; clean logs (no debug spew); graceful
  audio/controller failure re-verified in the packaged build.
- A clean-machine smoke test: the packaged build runs on a Windows machine
  without Visual Studio or development tooling installed.
- Final sign-off: full manual test matrix pass on the packaged build; known
  limitations documented honestly; the owner explicitly approves the release
  candidate.

## D. Non-goals

- No feature, content, or balance changes — release-blocking fixes only.
- No new platforms, installers beyond a simple archive/folder layout unless
  the owner requests one, auto-updaters, or telemetry (network features are
  prohibited by contract).
- No debug overlays, capture tooling, placeholder paths, or unlicensed assets
  in the shipped build.

## E. Dependencies

- M23 `complete (approved)`; all prior milestones `complete (approved)`.
- Access to a clean Windows machine (or equivalent clean environment) for the
  smoke test — owner-provided if Claude cannot verify.
- **Owner approval required for:** version numbering scheme, distribution
  format, and the final release-candidate acceptance.

## F. Proposed slices (provisional — refine before starting)

1. **Build/preset reproducibility** — `CMakePresets.json` with the supported
   MSVC/Ninja Release configuration; documented one-command build.
2. **Packaging** — scripted/CMake-driven staging of exe + `data/` + `assets/`
   + credits/licenses + player README; save/settings path verification;
   version metadata + icon.
3. **Release hygiene** — Release-build audit: debug overlay and capture
   tooling absent/disabled; logs clean; placeholder/licensing sweep against
   the manifest and credits records.
4. **Final validation** — clean-machine smoke test; full manual-matrix pass
   on the packaged build; known-limitations list; owner sign-off.

## G. Expected systems affected

- `CMakeLists.txt`, new `CMakePresets.json`, possible small packaging script
  under `cmake/` or `tools/`.
- Executable resources (icon, version info) — Windows-specific bits isolated
  per the platform rule.
- `README.md` (developer) and a player-facing README/credits in the package.
- No gameplay source changes expected.

## H. Data, schema, and save compatibility

- **No impact** on saves, settings, content schemas, seeds, or scoring. The
  packaged build must read/write the same user-data paths as the development
  build (verify — a packaged-vs-dev path split would be a release blocker).

## I. Automated validation

- Release-configuration configure/build/test: full suite green.
- Packaging validation: staged layout contains every required file; manifest
  IDs resolve inside the package; license record exists for every shipped
  asset; no debug artifacts present.
- Startup/exit smoke run of the packaged executable where the environment
  permits.

## J. Manual validation for the owner

- On a machine without Visual Studio: unpack, run, play a full loop (new
  game → town → dungeon → boss → score → save/load), exit cleanly.
- Verify saves/settings persist across relaunch in the packaged build.
- Full manual-test-matrix pass on the packaged build (not the dev build).
- Review credits/licenses and known limitations.
- Explicit release-candidate approval decision.

## K. Acceptance criteria

- The packaged build runs on a machine without Visual Studio installed.
- No debug presentation, capture tooling, unlicensed asset, or unintended
  placeholder ships.
- Version metadata, icon, credits, and licenses are present and correct.
- The final manual matrix is signed off on the packaged build.
- Known limitations are documented honestly.
- The owner explicitly approves the release candidate.

## L. Risks and failure modes

- **Works-on-dev-machine** — hidden dependencies (VS runtime, working-dir
  assumptions, absolute paths) that only the clean-machine test catches; run
  it early, not last.
- **Packaging drift** — hand-built packages rot; the staging step must be
  scripted/reproducible.
- **License gap discovered late** — the M21 provenance records are the
  defense; a gap here blocks release until resolved.
- **Last-minute "tiny" fixes** — release pressure invites unreviewed changes;
  release-blocking fixes only, each re-validated.
- **Path regression** — packaged build writing saves somewhere new would
  strand existing players' data; explicit verification required.

## M. Required documentation updates on completion

- `docs/milestones.md` — M24 and program completion status.
- This note (as-implemented + deviations).
- `README.md` — final build/run/packaging documentation.
- `docs/technical_design.md` — presets/packaging section.
- `docs/completion_roadmap.md` — program closure note.
- Completion report per `docs/milestone_completion_template.md`.

## N. Interim as-implemented record (2026-07-20 — engineering done)

**Presets & runtime (slice 1).** `CMakePresets.json`: `msvc-debug` and
`msvc-release` (Ninja, `cl` forced). The release preset links the
**static MSVC runtime** (`CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`,
`CMP0091` forced NEW so raylib matches) — the re-audit found the default
`/MD` build would not launch without the VC++ redistributable, exactly
the works-on-dev-machine trap §L warned about. Proof: the staged exe's
import table lists only `WINMM/KERNEL32/USER32/GDI32/SHELL32`.

**Version & identity (slice 2).** Single source `project(VERSION 0.9.0)`
(owner decision: 1.0.0 only after the M23 playtests pass) configured into
`generated/core/Version.hpp` — shown bottom-left on the title screen —
and `packaging/CrystalDungeons.rc.in` (VERSIONINFO + icon).
`tools/asset_gen/generate_icon.ps1` builds `packaging/crystal.ico`
(16/32/48/256, classic 32bpp DIB entries) from the approved emblem.
Release builds cap raylib logging at warnings.

**Packaging (slice 2/3).** `tools/package.ps1`: preset build → stage
`CrystalDungeons.exe` + `data/` + `assets/` + player `README.txt` +
`LICENSES.txt` (raylib zlib, nlohmann MIT, original-content statement;
Catch2 is test-only and does not ship) → **validate** (all required
files; every manifest path resolves inside the package; no
.pdb/.ilk/.exp/.lib; capture strings absent from the exe; exe
ProductVersion matches the project version) → zip
`dist/CrystalDungeons-0.9.0-win64.zip` (6.1 MB). `dist/` is gitignored.

**Verification (slice 4, machine-side).** Staged exe launches from the
dist folder and exits clean (exe-relative `data`/`assets` resolution
confirmed); `--capture` is inert in the packaged build (starts the game,
never the tool); Debug and Release suites both 247/247.

**Remaining (gated on M23 + owner):** clean-machine smoke test on a
Windows box without dev tooling, the full manual-matrix pass on the
packaged build, the 0.9.0→1.0.0 bump, and the explicit release-candidate
acceptance. Roadmap closure notes wait for actual closure.
