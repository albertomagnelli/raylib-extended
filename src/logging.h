#pragma once

/// @file logging.h
/// Levelbasiertes Logging mit optionalem Callback.
///
/// Die Ausgabe erfolgt auf `std::cout`. Ist ein Callback gesetzt, ersetzt er
/// die Konsolenausgabe. Das Logging ist thread-sicher.

#include <atomic>
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#define FILENAME   (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define LogFatal   (logging::Logger{logging::Logger::LogLevel::FATAL,   FILENAME, __LINE__})
#define LogError   (logging::Logger{logging::Logger::LogLevel::ERROR,   FILENAME, __LINE__})
#define LogAlert   (logging::Logger{logging::Logger::LogLevel::ALERT,   FILENAME, __LINE__})
#define LogWarning (logging::Logger{logging::Logger::LogLevel::WARN,    FILENAME, __LINE__})
#define LogInfo    (logging::Logger{logging::Logger::LogLevel::INFO,    FILENAME, __LINE__})
#define LogDebug   (logging::Logger{logging::Logger::LogLevel::DEBUG,   FILENAME, __LINE__})
#define LogTrace   (logging::Logger{logging::Logger::LogLevel::TRACE,   FILENAME, __LINE__})

namespace logging {
  class Logger {
    public:
      enum class LogLevel : uint8_t {
        FATAL   = 0,
        ERROR   = 1,
        ALERT   = 2,
        WARN    = 3,
        INFO    = 4,
        DEBUG   = 5,
        TRACE   = 6,
        INVALID = 7
      };

      enum class OutColor : uint8_t {
        C_RESET = 0,
        C_RED = 1,
        C_GREEN = 2,
        C_YELLOW = 3,
        C_BLUE = 4,
        C_MAGENTA = 5,
        C_GREY = 6
      };

      /// Empfaengt die fertig formatierte Zeile (ohne Zeilenumbruch).
      using LogCallback = void (*)(LogLevel level, std::string_view line, void *userData);

      Logger(LogLevel logLevel, const std::string &filename, int32_t line);
      ~Logger();

      Logger(const Logger &) = delete;
      Logger &operator=(const Logger &) = delete;

      template<typename... TT>
      void operator()(const char *fmt_str, TT &&... args);

      template<typename... TT>
      void operator()(const std::string &fmt_str, TT &&... args);

      template<typename T>
      Logger &operator<<(const T &data);

      Logger &operator<<(std::ostream &(*f)(std::ostream &));

      /// Setzt den Schwellwert (Minimum) fuer die Ausgabe.
      static void setLogLevel(LogLevel level) noexcept;
      [[nodiscard]] static LogLevel logLevel() noexcept;
      [[nodiscard]] static bool logEnabled(LogLevel level) noexcept;

      /// Ersetzt die Konsolenausgabe. `nullptr` stellt die Konsole wieder her.
      static void setCallback(LogCallback callback, void *userData = nullptr) noexcept;

      /// Schreibt eine bereits formatierte Zeile.
      static void logMessage(LogLevel level, std::string_view message);

    private:
      template<typename... Args>
      void printFormat(const char *fmt_str, Args &&... args) {
        if (!logEnabled(currentLogLevel)) return;

        std::string formattedString;
        if constexpr (sizeof...(Args) == 0) formattedString = fmt_str;
        else formattedString = std::vformat(fmt_str, std::make_format_args(args...));

        emit(currentLogLevel, formattedString, currentFileName, currentLine);
      }

      static void emit(LogLevel level, std::string_view message, std::string_view filename, int32_t line);

      static OutColor    currentColor;
      static LogLevel    currentLogLevel;
      static std::atomic<LogLevel> maxLogLevel;
      static int32_t     currentLine;
      static std::string currentFileName;

      static LogCallback callback;
      static void       *callbackUserData;

      static std::mutex mutex;

      std::lock_guard<std::mutex> lock{mutex};
  };

  template<typename... TT>
  void Logger::operator()(const char *fmt_str, TT &&... args) {
    printFormat(fmt_str, std::forward<TT>(args)...);
  }

  template<typename... TT>
  void Logger::operator()(const std::string &fmt_str, TT &&... args) {
    printFormat(fmt_str.c_str(), std::forward<TT>(args)...);
  }

  template<typename T>
  Logger &Logger::operator<<(const T &data) {
    if (!logEnabled(currentLogLevel)) return *this;

    std::ostringstream dataStream;
    dataStream << data;

    if (dataStream.str().empty()) return *this;
    printFormat("{}", dataStream.str());

    return *this;
  }
}
