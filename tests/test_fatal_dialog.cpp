#include <catch2/catch_test_macros.hpp>

#include <string_view>

// Regression guard for the fatal-dialog build break: <windows.h> was included
// directly in main.cpp, so the Win32 min/max macros broke std::max in
// FadeController.hpp and the GDI/USER Rectangle + DrawText declarations
// collided with raylib's. Including the platform header first, then a
// raylib-facing header and a std::max user, reproduces that exact TU shape --
// this file failing to compile is the regression.
#include "platform/FatalDialog.hpp"

#include "core/FadeController.hpp"
#include "render/VirtualScreen.hpp"

#if defined(min) || defined(max)
#error "FatalDialog.hpp leaked the Windows min/max macros"
#endif
#if defined(Rectangle) || defined(DrawText) || defined(CloseWindow)
#error "FatalDialog.hpp leaked Windows GDI/USER symbols"
#endif

TEST_CASE("fatal dialog: header is isolated from Win32 and links", "[platform]") {
    // Taking the address proves the symbol links without opening a modal.
    void (*const dialog)(std::string_view) noexcept = &cd::platform::showFatalDialog;
    REQUIRE(dialog != nullptr);

    // The dialog is called while unwinding a fatal exception, so it must not
    // be able to throw a second one.
    STATIC_REQUIRE(noexcept(cd::platform::showFatalDialog(std::string_view{})));
}

TEST_CASE("fatal dialog: raylib Rectangle survives the platform header", "[platform]") {
    // Would not compile if wingdi.h's Rectangle() function declaration were
    // visible: raylib's Rectangle is a struct with float fields.
    const Rectangle rect{1.0f, 2.0f, 3.0f, 4.0f};
    REQUIRE(rect.width == 3.0f);

    // std::max in a header that <windows.h> previously broke.
    cd::FadeController fade;
    fade.start(0.2f);
    fade.update(10.0f);
    REQUIRE_FALSE(fade.active());
}
