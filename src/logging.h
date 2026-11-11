#pragma once

/// @file logging.h
/// Logstufen und die zentrale Logging-Pipeline der Engine.
///
/// Alle Logausgabe laeuft ueber `logMessage()`. Optional koennen eine
/// Logdatei und/oder ein Callback gesetzt werden; ohne beides wird farbig
/// auf stderr ausgegeben.

#include "error.h"

#include <string_view>


    /// Logstufen, aufsteigend nach Prioritaet.
    /// `off` unterdrueckt samtliche Ausgabe.
    enum class LogLevel : u8 {
        trace = 0,  ///< Trace-Logging, nur fuer internen Gebrauch.
        debug,      ///< Debug-Logging, in Release-Builds deaktivieren.
        info,       ///< Info-Logging, Programmablauf.
        warning,    ///< Warnungen, recoverable Fehler.
        error,      ///< Fehler, unrecoverable Fehler.
        fatal,      ///< Fataler Fehler, Programmabbruch.
        off,        ///< Logausgabe komplett deaktiviert.
    };

    /// Callback-Signatur der zentralen Logging-Pipeline.
    /// `line` ist die fertig formatierte Zeile inklusive Zeitstempel.
    using LogCallback = void (*)(LogLevel level, std::string_view line, void* userData);

    /// Lesbare Bezeichnung einer Logstufe (`"TRACE"`, `"WARN"` usw.).
    [[nodiscard]] std::string_view toString(LogLevel level) noexcept;

    /// Setzt die minimale Logstufe. Darunter liegende Meldungen werden verworfen.
    void setLogLevel(LogLevel level) noexcept;

    /// Aktuelle minimale Logstufe.
    [[nodiscard]] LogLevel logLevel() noexcept;

    /// True, wenn Meldungen dieser Stufe ausgegeben wuerden.
    [[nodiscard]] bool logEnabled(LogLevel level) noexcept;

    /// Registriert einen Callback, der die formatierte Zeile erhaelt.
    /// `callback == nullptr` entfernt den Callback.
    void setLogCallback(LogCallback callback, void* userData) noexcept;

    /// Meldungen zusaetzlich in `path` schreiben. Leerer Pfad schliesst die Datei.
    [[nodiscard]] Status setLogFile(std::string_view path);

    /// Logdatei schliessen.
    void closeLogFile() noexcept;

    /// Zentrale Logausgabe: Zeitstempel, Stufe, Nachricht.
    void logMessage(LogLevel level, std::string_view message);
