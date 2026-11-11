#pragma once

/// @file logging.h
/// Levelbasiertes Logging mit optionalem Callback- und Datei-Sink.
///
/// Standardausgabe ist `stderr`. Ist ein Callback gesetzt, ersetzt er die
/// Konsolenausgabe (der Datei-Sink bleibt aktiv). Logging ist thread-sicher.
///
/// Hinweis: Log-Funktionen allokieren und sind daher die einzige Ausnahme von
/// der `noexcept`-Regel des Engine-Codes.

#include <format>
#include <string_view>
#include <utility>

#include "types.h"
#include "error.h"

enum class LogLevel : u8 {
    trace = 0,
    debug,
    info,
    warn,
    error,
    fatal,
    off,  ///< Schaltet Logging ab (nur als Grenzwert sinnvoll).
};

[[nodiscard]] std::string_view toString(LogLevel level) noexcept;

/// Empfaengt die fertig formatierte Zeile (ohne Zeilenumbruch).
using LogCallback = void (*)(LogLevel level, std::string_view line, void* userData);

void setLogLevel(LogLevel level) noexcept;
[[nodiscard]] LogLevel logLevel() noexcept;
[[nodiscard]] bool logEnabled(LogLevel level) noexcept;

/// Ersetzt die Konsolenausgabe. `nullptr` stellt `stderr` wieder her.
void setLogCallback(LogCallback callback, void* userData = nullptr) noexcept;

/// Oeffnet eine Logdatei (wird bei jedem Aufruf neu angelegt).
[[nodiscard]] Status setLogFile(std::string_view path);
void closeLogFile() noexcept;

/// Schreibt eine bereits formatierte Zeile.
void logMessage(LogLevel level, std::string_view message);

template <class... Args>
void log(LogLevel level, std::format_string<Args...> formatString, Args&&... args) {
    if (!logEnabled(level)) {
        return;
    }
    logMessage(level, std::format(formatString, std::forward<Args>(args)...));
}

template <class... Args>
void trace(std::format_string<Args...> formatString, Args&&... args) {
    log(LogLevel::trace, formatString, std::forward<Args>(args)...);
}

template <class... Args>
void debug(std::format_string<Args...> formatString, Args&&... args) {
    log(LogLevel::debug, formatString, std::forward<Args>(args)...);
}

template <class... Args>
void info(std::format_string<Args...> formatString, Args&&... args) {
    log(LogLevel::info, formatString, std::forward<Args>(args)...);
}

template <class... Args>
void warn(std::format_string<Args...> formatString, Args&&... args) {
    log(LogLevel::warn, formatString, std::forward<Args>(args)...);
}

template <class... Args>
void error(std::format_string<Args...> formatString, Args&&... args) {
    log(LogLevel::error, formatString, std::forward<Args>(args)...);
}

template <class... Args>
void fatal(std::format_string<Args...> formatString, Args&&... args) {
    log(LogLevel::fatal, formatString, std::forward<Args>(args)...);
}
