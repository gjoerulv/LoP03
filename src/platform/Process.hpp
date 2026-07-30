#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// M60 (CrystalForge test runner): spawn a child process and stream its output.
// Windows-only behind this interface, per the platform-isolation rule. The
// implementation is compiled ONLY into crystal_editor_core — the game binary
// never links it, so game code structurally cannot execute processes (the
// CLAUDE.md "no shell execution from game code" rule holds by construction,
// not convention).

namespace cd::platform {

class ProcessRunner {
public:
    ProcessRunner();
    ~ProcessRunner();  // terminates a still-running child, then joins the reader
    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;

    // Launches `exe` with `args` (quoted individually), capturing stdout+stderr.
    // False + `error` when the process could not start. One launch per runner.
    bool start(const std::filesystem::path& exe, const std::vector<std::string>& args,
               std::string& error);

    bool running() const;

    // Complete output lines received since the last drain (thread-safe; the
    // caller polls once per frame).
    std::vector<std::string> drainLines();

    // Valid once running() is false after a successful start().
    int exitCode() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace cd::platform
