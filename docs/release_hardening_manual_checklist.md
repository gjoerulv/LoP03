# Release-hardening manual validation

Target baseline: `356619d64d4511c7f047bef1a4ca82d1df561595` plus the release-hardening patch.

This checklist records work the owner must perform. It does **not** claim any result in advance.

## Automated build and package checks

From an x64 Visual Studio developer shell:

```powershell
cmake --preset msvc-debug
cmake --build --preset debug
ctest --preset debug

cmake --preset msvc-release
cmake --build --preset release
ctest --preset release

powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

Also verify the multi-configuration path:

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Release
```

Inspect both Release executables and confirm:

- F1 has no debug-overlay effect.
- `--capture` is unavailable.
- no capture output can be generated.
- the packaged executable is AMD64.
- the package script rejects forbidden debug/capture markers.

## Persistence failure checks

- Save a valid slot, save again, and confirm `slot*.json.bak` holds the previous valid generation.
- Confirm settings, tutorial progress, and scoreboard data survive normal restart.
- Make the sibling `.tmp` path unwritable or non-removable; confirm a failed write leaves the previous destination intact.
- Run the malformed-scoreboard automated test and confirm it proves a failed load preserves previously valid in-memory entries.

## Diagnostics

- Start and exit the packaged build; confirm a timestamped log is written under the user-data `logs` directory.
- Trigger or temporarily inject a startup exception; confirm the GUI error dialog names the diagnostic log path.
- Confirm logging works when failure occurs before the raylib window or audio device is initialized.

## Final release evidence

Record build SHA, package version, tester, date, display mode, keyboard/controller used, and results in `docs/manual_test_matrix.md`. Keep M23/M24 as awaiting approval until the full packaged-build matrix and clean-machine smoke test pass.
