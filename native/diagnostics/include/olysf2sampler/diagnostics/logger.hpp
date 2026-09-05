#pragma once
// native/diagnostics — logging estructurado y métricas.
// Responsabilidad única: registrar/reportar; nunca debe usarse
// "logging pesado" dentro del audio callback (Fase 1 §13). El logging
// desde el hot path debe limitarse a contadores atómicos consultados
// fuera de ese contexto (ver AudioMetrics).

#include <string_view>

namespace olysf2sampler::diagnostics {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
};

class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(LogLevel level, std::string_view tag, std::string_view message) noexcept = 0;
};

}  // namespace olysf2sampler::diagnostics
