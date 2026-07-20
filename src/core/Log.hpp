#pragma once

#include <filesystem>
#include <string_view>

namespace cd::log {

// Creates a timestamped persistent log under logDirectory. Logging remains
// usable through raylib even when persistent-file initialization fails.
bool initialize(const std::filesystem::path& logDirectory);
void shutdown();
std::filesystem::path currentLogPath();

void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);

} // namespace cd::log
