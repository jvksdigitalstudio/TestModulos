#pragma once
// Contadores realtime-safe (atómicos) que el audio callback puede
// incrementar sin bloquear; se leen desde fuera del hot path (p.ej.
// una pantalla de diagnóstico en la app).

#include <atomic>
#include <cstdint>

namespace olysf2sampler::diagnostics {

struct AudioMetrics {
    std::atomic<std::uint64_t> callbackCount{0};
    std::atomic<std::uint64_t> underrunCount{0};
    std::atomic<std::uint64_t> xrunCount{0};
};

}  // namespace olysf2sampler::diagnostics
