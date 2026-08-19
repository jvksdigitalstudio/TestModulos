#pragma once
// native/memory — gestión de memoria para el hot path de audio.
// Responsabilidad única: proveer buffers reutilizables sin allocación
// dinámica dentro del audio callback (Fase 1 §13).

#include <cstddef>

namespace olysf2sampler::memory {

/// Pool de buffers de tamaño fijo, pre-asignados. `acquire`/`release`
/// deben ser realtime-safe (lock-free o wait-free), nunca llamar a
/// new/delete/malloc en el hot path.
class BufferPool {
public:
    virtual ~BufferPool() = default;

    virtual void preallocate(std::size_t bufferCount, std::size_t bufferSizeFrames) = 0;

    /// Devuelve nullptr si el pool está agotado (nunca aloja bajo demanda
    /// dentro de esta llamada si se invoca desde el audio thread).
    virtual float* acquire() noexcept = 0;
    virtual void release(float* buffer) noexcept = 0;
};

}  // namespace olysf2sampler::memory
