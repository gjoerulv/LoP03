#include <cstdio>
#include <exception>
#include <string_view>

#include "capture/CaptureRunner.hpp"
#include "core/Application.hpp"

int main(int argc, char** argv) {
#ifdef CRYSTAL_CAPTURE
    // Deterministic native-res captures (M23); dev builds only.
    if (argc >= 2 && std::string_view(argv[1]) == "--capture") {
        return cd::capture::run(argc >= 3 ? argv[2] : "captures");
    }
#else
    (void)argc;
    (void)argv;
#endif
    try {
        cd::Application app;
        app.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "Fatal error: unknown exception\n");
        return 1;
    }
    return 0;
}
