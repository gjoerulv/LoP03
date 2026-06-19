#include "platform/Paths.hpp"

#include <cstdlib>

namespace cd::paths {

namespace fs = std::filesystem;

namespace {

// Reads an environment variable, returning std::nullopt if unset/empty.
std::optional<std::string> envVar(const char* name) {
#if defined(_MSC_VER)
    // getenv triggers a deprecation warning under MSVC; use the safe variant.
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
#else
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

constexpr const char* kAppFolderName = "CrystalDungeons";

}  // namespace

std::optional<fs::path> sanitizeRelative(std::string_view relative) {
    if (relative.empty()) {
        return std::nullopt;
    }

    fs::path p = fs::path(relative).lexically_normal();

    // Must be relative with no root name (drive) or root directory.
    if (p.is_absolute() || p.has_root_name() || p.has_root_directory()) {
        return std::nullopt;
    }

    // Reject any traversal component.
    for (const auto& part : p) {
        if (part == "..") {
            return std::nullopt;
        }
    }

    // Normalization can collapse something like "a/.." to "." -> reject empties.
    if (p.empty() || p == ".") {
        return std::nullopt;
    }

    return p;
}

fs::path userDataDir() {
#if defined(_WIN32)
    if (auto appdata = envVar("APPDATA")) {
        return fs::path(*appdata) / kAppFolderName;
    }
#else
    if (auto xdg = envVar("XDG_DATA_HOME")) {
        return fs::path(*xdg) / kAppFolderName;
    }
    if (auto home = envVar("HOME")) {
        return fs::path(*home) / ".local" / "share" / kAppFolderName;
    }
#endif
    return fs::path(".") / kAppFolderName;
}

}  // namespace cd::paths
