#include "platform/FatalDialog.hpp"

#include <climits>
#include <cstdio>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// NOMINMAX keeps the min/max function macros out of this TU. NOUSER is
// deliberately NOT defined: MessageBox lives in the USER API.
#define NOMINMAX
#include <windows.h>
#endif

namespace cd::platform {

namespace {

#ifdef _WIN32
constexpr UINT kDialogFlags = MB_OK | MB_ICONERROR | MB_SETFOREGROUND;

// UTF-8 to UTF-16 for the wide MessageBox. Returns an empty string when the
// text cannot be converted, which the caller treats as "fall back to ANSI".
std::wstring widen(std::string_view utf8) {
    if (utf8.empty() || utf8.size() > static_cast<std::size_t>(INT_MAX)) {
        return {};
    }
    const int length = static_cast<int>(utf8.size());
    const int wideLength =
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), length, nullptr, 0);
    if (wideLength <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8.data(), length, wide.data(),
                            wideLength) <= 0) {
        return {};
    }
    return wide;
}
#endif

} // namespace

void showFatalDialog(std::string_view message) noexcept {
    try {
#ifdef _WIN32
        const std::wstring wide = widen(message);
        if (!wide.empty()) {
            MessageBoxW(nullptr, wide.c_str(), L"Crystal Dungeons - Fatal Error",
                        kDialogFlags);
            return;
        }
        // Conversion failed (or the message was empty): a narrow copy still
        // gets something in front of the player.
        const std::string narrow(message);
        MessageBoxA(nullptr, narrow.c_str(), "Crystal Dungeons - Fatal Error",
                    kDialogFlags);
#else
        std::fprintf(stderr, "%.*s\n", static_cast<int>(message.size()),
                     message.data());
#endif
    } catch (...) {
        // Reporting a fatal error must never raise a second one.
    }
}

} // namespace cd::platform
