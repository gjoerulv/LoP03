#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

#include "capture/CaptureRunner.hpp"
#include "core/Application.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"
#include "platform/FatalDialog.hpp"
#include "platform/Paths.hpp"

#if defined(CRYSTAL_SHIPPING_BUILD) && \
    (defined(CRYSTAL_DEBUG_OVERLAY) || defined(CRYSTAL_CAPTURE))
#error "Shipping builds must not contain debug-overlay or capture tooling"
#endif

namespace {

// Composes the player-facing text and hands it to the platform dialog. The raw
// message is already in the log by this point; this adds the log location so a
// player can find the diagnostics.
void showFatalError(std::string_view message) noexcept {
    try {
        std::string text = "Crystal Dungeons could not continue.\n\n";
        text.append(message);
        const std::filesystem::path logPath = cd::log::currentLogPath();
        if (!logPath.empty()) {
            text += "\n\nDiagnostic log:\n" + logPath.string();
        }
        cd::platform::showFatalDialog(text);
    } catch (...) {
        // Composing the friendly text failed; still surface the raw message.
        cd::platform::showFatalDialog(message);
    }
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
