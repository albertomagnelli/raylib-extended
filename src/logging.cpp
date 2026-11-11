#include "loggging.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>

#if defined(_WIN32)
#    include <windows.h>
#endif


namespace {

constexpr std::array<std::string_view, 7> kLevelNames{"TRACE", "DEBUG", "INFO", "WARN",
                                                      "ERROR", "FATAL", "OFF"};

constexpr std::array<const char*, 7> kLevelColors{"\x1b[90m",   "\x1b[36m", "\x1b[32m", "\x1b[33m",
                                                  "\x1b[31m",   "\x1b[1;31m", ""};

constexpr auto kColorReset = "\x1b[0m";

std::atomic<LogLevel> g_level{LogLevel::info};
std::mutex g_mutex;
LogCallback g_callback = nullptr;
void* g_callbackUser = nullptr;
std::ofstream g_file;

[[nodiscard]] constexpr std::size_t levelIndex(LogLevel level) noexcept {
    const auto index = static_cast<std::size_t>(level);
    return index < kLevelNames.size() ? index : kLevelNames.size() - 1;
}

void ensureConsoleSetup() noexcept {
#if defined(_WIN32)
    static std::once_flag flag;
    std::call_once(flag, [] {
        for (const DWORD which : {STD_ERROR_HANDLE, STD_OUTPUT_HANDLE}) {
            HANDLE handle = ::GetStdHandle(which);
            DWORD mode = 0;
            if (handle != INVALID_HANDLE_VALUE && ::GetConsoleMode(handle, &mode) != 0) {
                ::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
    });
#endif
}

void appendTimestamp(std::string& out) {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm local{};
#if defined(_WIN32)
    ::localtime_s(&local, &seconds);
#else
    ::localtime_r(&seconds, &local);
#endif

    std::format_to(std::back_inserter(out), "{:02}:{:02}:{:02}.{:03}", local.tm_hour, local.tm_min,
                   local.tm_sec, static_cast<int>(millis.count()));
}

}  // namespace

std::string_view toString(LogLevel level) noexcept { return kLevelNames[levelIndex(level)]; }

void setLogLevel(LogLevel level) noexcept { g_level.store(level, std::memory_order_relaxed); }

LogLevel logLevel() noexcept { return g_level.load(std::memory_order_relaxed); }

bool logEnabled(LogLevel level) noexcept {
    return level != LogLevel::off && level >= g_level.load(std::memory_order_relaxed);
}

void setLogCallback(LogCallback callback, void* userData) noexcept {
    const std::lock_guard lock(g_mutex);
    g_callback = callback;
    g_callbackUser = userData;
}

Status setLogFile(std::string_view path) {
    const std::lock_guard lock(g_mutex);

    if (g_file.is_open()) {
        g_file.close();
    }

    g_file.open(std::string{path}, std::ios::out | std::ios::trunc);
    if (!g_file.is_open()) {
        return std::unexpected(
            makeError(ErrorCode::ioFailure, std::format("Logdatei '{}' nicht beschreibbar", path)));
    }

    return {};
}

void closeLogFile() noexcept {
    const std::lock_guard lock(g_mutex);

    if (g_file.is_open()) {
        g_file.close();
    }
}

void logMessage(LogLevel level, std::string_view message) {
    if (!logEnabled(level)) {
        return;
    }

    ensureConsoleSetup();

    const std::size_t index = levelIndex(level);

    std::string line;
    line.reserve(message.size() + 40);
    line.push_back('[');
    appendTimestamp(line);
    line.append("] [");
    line.append(kLevelNames[index]);
    line.append("] ");
    line.append(message);

    const std::lock_guard lock(g_mutex);

    if (g_file.is_open()) {
        g_file << line << '\n';
        g_file.flush();
    }

    if (g_callback != nullptr) {
        g_callback(level, line, g_callbackUser);
        return;
    }

    std::fputs(kLevelColors[index], stderr);
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fputs(kColorReset, stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}