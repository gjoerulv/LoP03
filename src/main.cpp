#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "capture/CaptureRunner.hpp"
#include "core/Application.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"
#include "platform/Paths.hpp"

#if defined(CRYSTAL_SHIPPING_BUILD) && \
    (defined(CRYSTAL_DEBUG_OVERLAY) || defined(CRYSTAL_CAPTURE))
#error "Shipping builds must not contain debug-overlay or capture tooling"
#endif

namespace {

void showFatalError(std::string_view message) {
    std::string text = "Crystal Dungeons could not continue.\n\n";
    text.append(message);
    const std::filesystem::path logPath = cd::log::currentLogPath();
    if (!logPath.empty()) {
        text += "\n\nDiagnostic log:\n" + logPath.string();
    }
#ifdef _WIN32
    MessageBoxA(nullptr, text.c_str(), "Crystal Dungeons - Fatal Error", MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "%s\n", text.c_str());
#endif
}

} // namespace

int main(int argc, char** argv) {
    try {
#ifdef CRYSTAL_CAPTURE
        // Deterministic native-res captures (M23); dev builds only.
        if (argc >= 2 && std::string_view(argv[1]) == "--capture") {
            return cd::capture::run(argc >= 3 ? argv[2] : "captures");
        }
#else
        (void)argc;
        (void)argv;
#endif

        const std::filesystem::path logDirectory = cd::paths::userDataDir() / "logs";
        cd::log::initialize(logDirectory);
        cd::log::info(std::string("Crystal Dungeons ") + cd::version::kString + " starting");

        cd::Application app;
        app.run();

        cd::log::info("Crystal Dungeons shutdown complete");
        cd::log::shutdown();
        return 0;
    } catch (const std::exception& e) {
        const std::string message = std::string("Fatal error: ") + e.what();
        cd::log::error(message);
        showFatalError(message);
        cd::log::shutdown();
        return 1;
    } catch (...) {
        constexpr std::string_view message = "Fatal error: unknown exception";
        cd::log::error(message);
        showFatalError(message);
        cd::log::shutdown();
        return 1;
    }
}
