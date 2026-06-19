#include <cstdio>
#include <exception>

#include "core/Application.hpp"

int main() {
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
