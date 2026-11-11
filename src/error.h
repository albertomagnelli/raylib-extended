#pragma once

/// @file error.h
/// Fehlercodes und das Result-Typalias der Engine.
///
/// Alle falliblen Operationen geben `Result<T>` (`std::expected`) zurueck.
/// Exceptions sind im Engine-Code nicht Teil der Fehlerbehandlung.

#include "types.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>


    /// Grobe Fehlerklassifizierung. Feine Details stehen in `Error::message`.
    enum class ErrorCode : u8 {
        none = 0,       ///< Kein Fehler.
        cancelled,      ///< Vorgang vom Aufrufer abgebrochen.
        notFound,       ///< Ressource existiert nicht.
        accessDenied,   ///< Fehlende Berechtigung.
        ioFailure,      ///< Datei-/Geraetezugriff fehlgeschlagen.
        invalidArgument,///< Ungueltiger Parameter.
        invalidData,    ///< Daten sind syntaktisch ok, aber semantisch falsch.
        parseFailure,   ///< Daten konnten nicht geparst werden.
        unsupported,    ///< Feature wird nicht unterstuetzt.
        outOfMemory,    ///< Speicher erschoepft.
        glFailure,      ///< OpenGL-Fehler.
        deviceLost,     ///< GPU-Geraet verloren.
        timeout,        ///< Zeitueberschreitung.
    };

    /// Fehlerwert. `Error{}` bedeutet Erfolg (`code == ErrorCode::none`).
    struct Error {
        ErrorCode code = ErrorCode::none;
        std::string message{};

        [[nodiscard]] constexpr bool ok() const noexcept { return code == ErrorCode::none; }

        [[nodiscard]] constexpr explicit operator bool() const noexcept { return !ok(); }
    };

    /// Erzeugt einen Fehler. Der Text wird kopiert.
    [[nodiscard]] Error makeError(ErrorCode code, std::string_view message);

    /// Lesbare Bezeichnung eines Fehlercodes (`"notFound"` usw.).
    [[nodiscard]] std::string_view toString(ErrorCode code) noexcept;

    /// Ergebnis einer Operation mit Wert.
    template <class T>
    using Result = std::expected<T, Error>;

    /// Ergebnis einer Operation ohne Wert.
    using Status = std::expected<void, Error>;
