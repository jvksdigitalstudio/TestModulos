#pragma once
// native/audio — ciclo de vida del stream de audio y ruteo del
// callback hacia el sampler. Responsabilidad única: orquestar
// platform::AudioBackend y entregar/recibir buffers; no implementa
// DSP ni conoce SoundFont.

#include <cstddef>
#include <memory>

#include "olysf2sampler/platform/audio_backend.hpp"

namespace olysf2sampler::audio {

struct AudioSessionConfig {
    int sampleRate{48000};
    int framesPerBuffer{0};
};

/// Fachada del subsistema de audio. Conecta un platform::AudioBackend
/// con la fuente real de audio (típicamente sampler::SamplerEngine,
/// inyectado por quien ensambla el motor — no por este tipo).
class AudioEngine {
public:
    virtual ~AudioEngine() = default;

    virtual bool start(const AudioSessionConfig& config) noexcept = 0;
    virtual void stop() noexcept = 0;

    virtual void setRenderCallback(olysf2sampler::platform::AudioRenderCallback callback) = 0;

    virtual double currentLatencyMillis() const noexcept = 0;
};

/// Construye la implementación real. `backend` se inyecta (no se
/// construye internamente) precisamente para poder probar
/// `AudioEngine` en host con un `AudioBackend` de prueba, sin
/// necesitar Oboe ni un dispositivo Android — ver
/// tests/audio/audio_engine_test.cpp.
std::unique_ptr<AudioEngine> createAudioEngine(
    std::unique_ptr<olysf2sampler::platform::AudioBackend> backend);

}  // namespace olysf2sampler::audio
