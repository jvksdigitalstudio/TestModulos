#pragma once
// native/sampler/engine — fachada del subsistema sampler.
// Responsabilidad única: orquestar voice_manager + mapping para
// procesar eventos de nota y producir bloques de audio. Es el único
// tipo de este módulo expuesto más allá de OlySf2 Sampler (a través de la
// frontera JNI hacia api/).

#include <cstddef>
#include <cstdint>
#include <memory>

#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/voice.hpp"
#include "olysf2sampler/sampler/voice_manager.hpp"

namespace olysf2sampler::sampler {

class SamplerEngine {
public:
    virtual ~SamplerEngine() = default;

    /// Registra la fuente de audio real (PCM) para un `sampleId`. Debe
    /// llamarse FUERA del audio thread (típicamente al cargar un
    /// SoundFont), nunca desde dentro de `renderBlock`. `source` es una
    /// referencia no propietaria — quien la registra debe mantener el
    /// buffer PCM vivo mientras el sample siga registrado.
    virtual void loadSample(std::uint64_t sampleId, const SampleSource& source) noexcept = 0;

    virtual void noteOn(int midiNote, int velocity) noexcept = 0;
    virtual void noteOff(int midiNote) noexcept = 0;

    /// Volumen maestro lineal (0.0 = silencio, 1.0 = sin cambio, >1.0
    /// amplifica). Llamable desde CUALQUIER hilo (control thread o
    /// audio thread) — internamente usa `std::atomic<float>`, el
    /// patrón estándar para parámetros continuos en audio en tiempo
    /// real (más liviano que la cola SPSC usada para eventos de nota
    /// discretos; ver docs/architecture/THREADING.md).
    virtual void setMasterVolume(float linearGain) noexcept = 0;

    /// Filtro LowPass aplicado a todas las voces. `cutoffHz` en Hz,
    /// `resonance` como factor Q (ver dsp::createBiquadFilter). Mismo
    /// patrón thread-safe que `setMasterVolume`.
    virtual void setFilterCutoff(float cutoffHz, float resonance) noexcept = 0;

    /// Llamado desde el audio callback (olysf2sampler::audio). Debe ser
    /// realtime-safe end-to-end: no debe alojar memoria ni bloquear.
    virtual void renderBlock(float* outputBuffer, std::size_t frameCount) noexcept = 0;

    virtual void setMapping(KeyVelocityMapping* mapping) noexcept = 0;

    /// Cuántos eventos de nota (noteOn/noteOff) se descartaron desde
    /// la creación del motor porque la cola control→audio estaba
    /// llena (productor más rápido que el consumidor de audio,
    /// situación anómala). Fase A.1 §22: antes esto se descartaba sin
    /// ningún diagnóstico posible; ahora es observable. Llamable desde
    /// cualquier hilo (atómico, sin bloqueo). No resetea el contador.
    virtual std::uint64_t droppedNoteEventCount() const noexcept = 0;
};

/// Construye la implementación real, orquestando createVoiceManager()
/// + la fuente de audio registrada vía loadSample().
///
/// `maxPolyphony`: número máximo de voces simultáneas del motor (ver
/// Fase A.1 §17 — no debe tratarse como una constante de producto).
/// El valor por defecto (32) es solo un punto de partida razonable
/// para uso interactivo típico; el llamador (p.ej. el host adapter,
/// según el "performance profile"/"device capability" del host) puede
/// pasar un valor distinto. Internamente saturado a un límite
/// estructural fijo (kMaxSupportedPolyphony, ver sampler_engine_impl.cpp)
/// que garantiza que ninguna voz adquirida quede sin poder rastrearse.
std::unique_ptr<SamplerEngine> createSamplerEngine(float sampleRateHz,
                                                    std::size_t maxPolyphony = 32);

}  // namespace olysf2sampler::sampler
