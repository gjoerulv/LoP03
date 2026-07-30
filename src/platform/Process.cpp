#include "platform/Process.hpp"

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cd::platform {

#ifdef _WIN32

namespace {

// One argument, quoted for CreateProcess's command line (backslash-doubling
// before quotes per the MSVC parsing rules; our arguments are plain paths and
// Catch2 specs, but correctness is cheap).
std::wstring quoteArg(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    int backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"') {
            out.append(static_cast<std::size_t>(backslashes) * 2 + 1, L'\\');
            out += L'"';
        } else {
            out.append(static_cast<std::size_t>(backslashes), L'\\');
            out += c;
        }
        backslashes = 0;
    }
    out.append(static_cast<std::size_t>(backslashes) * 2, L'\\');
    out += L'"';
    return out;
}

std::wstring widen(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<std::size_t>(len > 0 ? len - 1 : 0), L'\0');
    if (len > 1) {
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    }
    return out;
}

}  // namespace

struct ProcessRunner::Impl {
    HANDLE process = nullptr;
    HANDLE readPipe = nullptr;
    std::thread reader;
    std::mutex mutex;
    std::vector<std::string> lines;
    std::string partial;
    std::atomic<bool> done{false};
    std::atomic<int> exitCode{-1};

    void pushChunk(const char* data, DWORD size) {
        std::lock_guard<std::mutex> lock(mutex);
        for (DWORD i = 0; i < size; ++i) {
            const char c = data[i];
            if (c == '\n') {
                if (!partial.empty() && partial.back() == '\r') {
                    partial.pop_back();
                }
                lines.push_back(std::move(partial));
                partial.clear();
            } else {
                partial.push_back(c);
            }
        }
    }

    void finishPartial() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!partial.empty()) {
            lines.push_back(std::move(partial));
            partial.clear();
        }
    }
};

ProcessRunner::ProcessRunner() : impl_(std::make_unique<Impl>()) {}

ProcessRunner::~ProcessRunner() {
    if (impl_->process != nullptr && !impl_->done.load()) {
        TerminateProcess(impl_->process, 1);
    }
    if (impl_->reader.joinable()) {
        impl_->reader.join();
    }
    if (impl_->readPipe != nullptr) {
        CloseHandle(impl_->readPipe);
    }
    if (impl_->process != nullptr) {
        CloseHandle(impl_->process);
    }
}

bool ProcessRunner::start(const std::filesystem::path& exe,
                          const std::vector<std::string>& args, std::string& error) {
    if (impl_->process != nullptr) {
        error = "runner already used";
        return false;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        error = "CreatePipe failed";
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmdLine = quoteArg(exe.wstring());
    for (const std::string& arg : args) {
        cmdLine += L' ';
        cmdLine += quoteArg(widen(arg));
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION pi{};
    // CreateProcessW may rewrite the command-line buffer; keep it mutable.
    std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back(L'\0');
    const BOOL ok = CreateProcessW(exe.wstring().c_str(), mutableCmd.data(), nullptr, nullptr,
                                   TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);  // the child holds the only write end now
    if (!ok) {
        CloseHandle(readPipe);
        error = "CreateProcess failed for " + exe.string() +
                " (code " + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(pi.hThread);
    impl_->process = pi.hProcess;
    impl_->readPipe = readPipe;

    Impl* impl = impl_.get();
    impl_->reader = std::thread([impl] {
        char buffer[4096];
        DWORD got = 0;
        while (ReadFile(impl->readPipe, buffer, sizeof(buffer), &got, nullptr) && got > 0) {
            impl->pushChunk(buffer, got);
        }
        impl->finishPartial();
        WaitForSingleObject(impl->process, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(impl->process, &code);
        impl->exitCode.store(static_cast<int>(code));
        impl->done.store(true);
    });
    return true;
}

bool ProcessRunner::running() const {
    return impl_->process != nullptr && !impl_->done.load();
}

std::vector<std::string> ProcessRunner::drainLines() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<std::string> out = std::move(impl_->lines);
    impl_->lines.clear();
    return out;
}

int ProcessRunner::exitCode() const { return impl_->exitCode.load(); }

#else  // !_WIN32 — the project targets Windows; other platforms report cleanly.

struct ProcessRunner::Impl {};
ProcessRunner::ProcessRunner() : impl_(std::make_unique<Impl>()) {}
ProcessRunner::~ProcessRunner() = default;
bool ProcessRunner::start(const std::filesystem::path&, const std::vector<std::string>&,
                          std::string& error) {
    error = "process spawning is only supported on Windows";
    return false;
}
bool ProcessRunner::running() const { return false; }
std::vector<std::string> ProcessRunner::drainLines() { return {}; }
int ProcessRunner::exitCode() const { return -1; }

#endif

}  // namespace cd::platform
