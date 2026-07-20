#include "core/Log.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

#include "raylib.h"

namespace cd::log {
namespace fs = std::filesystem;

namespace {

std::mutex gMutex;
std::ofstream gFile;
fs::path gPath;
constexpr std::size_t kRetainedLogs = 5;

std::tm localTime(std::time_t value) {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

std::string timestamp(const char* format) {
    const std::time_t now = std::time(nullptr);
    const std::tm local = localTime(now);
    std::ostringstream out;
    out << std::put_time(&local, format);
    return out.str();
}

void rotateLogs(const fs::path& directory) {
    std::error_code ec;
    std::vector<fs::directory_entry> logs;
    for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".log") {
            logs.push_back(*it);
        }
    }
    if (ec || logs.size() < kRetainedLogs) {
        return;
    }
    std::sort(logs.begin(), logs.end(), [](const auto& a, const auto& b) {
        std::error_code leftError;
        std::error_code rightError;
        return a.last_write_time(leftError) < b.last_write_time(rightError);
    });
    const std::size_t removeCount = logs.size() - kRetainedLogs + 1;
    for (std::size_t i = 0; i < removeCount; ++i) {
        fs::remove(logs[i].path(), ec);
        ec.clear();
    }
}

void write(int raylibLevel, std::string_view level, std::string_view message) {
    TraceLog(raylibLevel, "%.*s", static_cast<int>(message.size()), message.data());
    std::lock_guard lock(gMutex);
    if (gFile) {
        gFile << '[' << timestamp("%Y-%m-%d %H:%M:%S") << "] [" << level << "] "
              << message << '\n';
        gFile.flush();
    }
}

} // namespace

bool initialize(const fs::path& logDirectory) {
    std::lock_guard lock(gMutex);
    if (gFile.is_open()) {
        return true;
    }

    std::error_code ec;
    fs::create_directories(logDirectory, ec);
    if (ec) {
        gPath.clear();
        return false;
    }
    rotateLogs(logDirectory);
    gPath = logDirectory / ("CrystalDungeons-" + timestamp("%Y%m%d-%H%M%S") + ".log");
    gFile.open(gPath, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!gFile) {
        gPath.clear();
        return false;
    }
    gFile << '[' << timestamp("%Y-%m-%d %H:%M:%S") << "] [INFO] Log started\n";
    gFile.flush();
    return true;
}

void shutdown() {
    std::lock_guard lock(gMutex);
    if (gFile.is_open()) {
        gFile.flush();
        gFile.close();
    }
}

fs::path currentLogPath() {
    std::lock_guard lock(gMutex);
    return gPath;
}

void info(std::string_view message) { write(LOG_INFO, "INFO", message); }
void warn(std::string_view message) { write(LOG_WARNING, "WARN", message); }
void error(std::string_view message) { write(LOG_ERROR, "ERROR", message); }

} // namespace cd::log
