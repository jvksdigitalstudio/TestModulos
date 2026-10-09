#pragma once
// native/threading — primitivas de threading para audio
// en tiempo real. Responsabilidad única: definir el contrato del
// hilo de audio y su comunicación segura con worker threads, sin
// locks bloqueantes en el hot path (Fase 1 §13).

#include <cstddef>
#include <functional>

namespace olysf2sampler::threading {

using AudioCallback = std::function<void(float* outputBuffer, std::size_t frameCount)>;

/// Representa el hilo de audio real-time gestionado por la capa de
/// plataforma (Oboe/AAudio). Este tipo no implementa el motor de
/// audio; solo el contrato de cómo se conecta un callback.
class AudioThreadHandle {
public:
    virtual ~AudioThreadHandle() = default;

    virtual void setCallback(AudioCallback callback) = 0;
    virtual bool isRunning() const noexcept = 0;
};

}  // namespace olysf2sampler::threading
