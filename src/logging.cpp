#include "logging.h"

#include <ctime>
#include <iterator>
#include <string_view>

namespace {
  [[nodiscard]] constexpr std::string_view levelName(const logging::Logger::LogLevel level) noexcept {
    switch (level) {
      case logging::Logger::LogLevel::FATAL: return "FATAL";
      case logging::Logger::LogLevel::ERROR: return "ERROR";
      case logging::Logger::LogLevel::ALERT: return "ALERT";
      case logging::Logger::LogLevel::WARN:  return "WARN";
      case logging::Logger::LogLevel::INFO:  return "INFO";
      case logging::Logger::LogLevel::DEBUG: return "DEBUG";
      case logging::Logger::LogLevel::TRACE: return "TRACE";
      default:                               return "INVALID";
    }
  }

  [[nodiscard]] constexpr const char *levelColor(const logging::Logger::LogLevel level) noexcept {
    switch (level) {
      case logging::Logger::LogLevel::FATAL: return "\033[41m";
      case logging::Logger::LogLevel::ERROR: return "\033[31m";
      case logging::Logger::LogLevel::ALERT: return "\033[35m";
      case logging::Logger::LogLevel::WARN:  return "\033[33m";
      case logging::Logger::LogLevel::INFO:  return "\033[32m";
      case logging::Logger::LogLevel::DEBUG: return "\033[94m";
      case logging::Logger::LogLevel::TRACE: return "\033[36m";
      default:                               return "";
    }
  }
}

namespace logging {
  Logger::OutColor    Logger::currentColor{OutColor::C_RESET};
  std::atomic<Logger::LogLevel> Logger::maxLogLevel{LogLevel::INFO};
  Logger::LogLevel    Logger::currentLogLevel{LogLevel::INFO};
  int32_t             Logger::currentLine{-1};
  std::string         Logger::currentFileName{};
  Logger::LogCallback Logger::callback{nullptr};
  void               *Logger::callbackUserData{nullptr};
  std::mutex          Logger::mutex{};

  Logger::Logger(const LogLevel logLevel, const std::string &filename, const int32_t line) {
    currentLogLevel = logLevel;
    currentFileName = filename;
    currentLine     = line;
  }

  Logger::~Logger() {
    currentColor = OutColor::C_RESET;
  }

  Logger &Logger::operator<<(std::ostream &(*f)(std::ostream &)) {
    if (!logEnabled(currentLogLevel)) return *this;

    std::cout << f;

    return *this;
  }

  void Logger::setLogLevel(const LogLevel level) noexcept {
    maxLogLevel.store(level, std::memory_order_relaxed);
  }

  Logger::LogLevel Logger::logLevel() noexcept {
    return maxLogLevel.load(std::memory_order_relaxed);
  }

  bool Logger::logEnabled(const LogLevel level) noexcept {
    const auto threshold = maxLogLevel.load(std::memory_order_relaxed);
    return (threshold != LogLevel::INVALID) && (level != LogLevel::INVALID) && (level <= threshold);
  }

  void Logger::setCallback(const LogCallback logCallback, void *userData) noexcept {
    const std::lock_guard<std::mutex> guard(mutex);
    callback         = logCallback;
    callbackUserData = userData;
  }

  void Logger::logMessage(const LogLevel level, const std::string_view message) {
    if (!logEnabled(level)) return;

    const std::lock_guard<std::mutex> guard(mutex);
    emit(level, message, {}, -1);
  }

  void Logger::emit(const LogLevel level, const std::string_view message, const std::string_view filename, const int32_t line) {
    if (!logEnabled(level)) return;

    const time_t rawTime = std::time(nullptr);
    std::tm timeInfo{};
#if defined(_WIN32)
    localtime_s(&timeInfo, &rawTime);
#else
    localtime_r(&rawTime, &timeInfo);
#endif

    std::string out;
    out.reserve(message.size() + 80);
    std::format_to(std::back_inserter(out), "{:02}.{:02}.{:04} {:02}:{:02}:{:02} [{}]",
                   timeInfo.tm_mday, timeInfo.tm_mon + 1, timeInfo.tm_year + 1900,
                   timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec, levelName(level));

    if (!filename.empty()) {
      std::format_to(std::back_inserter(out), " file[{}] line [{:6}]", filename, line);
    }

    out.push_back(' ');
    out.append(message);

    if (callback != nullptr) {
      callback(level, out, callbackUserData);
      return;
    }

    const char *color = levelColor(level);
    if (*color != '\0') std::cout << color;
    std::cout << out;
    if (*color != '\0') std::cout << "\033[0m";
    std::cout << std::endl;
  }
}
