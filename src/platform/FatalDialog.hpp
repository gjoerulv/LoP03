#pragma once

#include <string_view>

// Native "the game could not continue" dialog. Deliberately narrow: the Win32
// headers stay inside FatalDialog.cpp so <windows.h> macros (min/max, and the
// GDI/USER Rectangle + DrawText declarations) can never collide with raylib in
// a translation unit that includes this header.

namespace cd::platform {

// Shows a modal error dialog on Windows; writes the message to stderr on other
// platforms. Called while handling a fatal exception, so it never throws and
// silently gives up rather than failing a second time.
void showFatalDialog(std::string_view message) noexcept;

} // namespace cd::platform
