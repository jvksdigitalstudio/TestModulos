#pragma once
// native/core — tipos base compartidos por todos los módulos de OlySf2 Sampler.
// Responsabilidad única de este header: definir un tipo de resultado
// uniforme (éxito/error) sin excepciones en el hot path de audio.

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace olysf2sampler::core {

/// Código de error estable, independiente del módulo que lo produce.
/// Los módulos concretos (soundfont, sampler, audio, ...) mapean sus
/// errores internos a este conjunto reducido para que la capa pública
/// (api/) no dependa de detalles internos.
enum class ErrorCode {
    Unknown = 0,
    InvalidArgument,
    NotInitialized,
    IoFailure,
    UnsupportedFormat,
    OutOfMemory,
    NotImplemented,
    MalformedInput,          // datos externos (p.ej. SF2) estructuralmente inválidos
    ResourceLimitExceeded,   // protección de agotamiento de recursos (§34)
};

struct Error {
    ErrorCode code{ErrorCode::Unknown};
    std::string message;
};

/// Result<T> minimalista: evita excepciones en rutas donde el costo o
/// la imprevisibilidad de una excepción no es aceptable (p.ej. cerca
/// del audio callback), sin necesitar una dependencia externa.
template <typename T>
class Result {
public:
    static Result<T> ok(T value) { return Result(std::move(value)); }
    static Result<T> fail(Error error) { return Result(std::move(error)); }

    [[nodiscard]] bool isOk() const noexcept { return std::holds_alternative<T>(storage_); }
    [[nodiscard]] bool isError() const noexcept { return !isOk(); }

    [[nodiscard]] const T& value() const& { return std::get<T>(storage_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(storage_)); }

    [[nodiscard]] const Error& error() const& { return std::get<Error>(storage_); }

private:
    explicit Result(T value) : storage_(std::move(value)) {}
    explicit Result(Error error) : storage_(std::move(error)) {}

    std::variant<T, Error> storage_;
};

/// Especialización para operaciones sin valor de retorno útil.
class VoidResult {
public:
    static VoidResult ok() { return VoidResult(std::nullopt); }
    static VoidResult fail(Error error) { return VoidResult(std::move(error)); }

    [[nodiscard]] bool isOk() const noexcept { return !error_.has_value(); }
    [[nodiscard]] bool isError() const noexcept { return error_.has_value(); }
    [[nodiscard]] const Error& error() const { return *error_; }

private:
    explicit VoidResult(std::optional<Error> error) : error_(std::move(error)) {}
    std::optional<Error> error_;
};

}  // namespace olysf2sampler::core
