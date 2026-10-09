#pragma once
// native/dsp/envelopes — generación de envolventes (ADSR y
// variantes). Responsabilidad única: producir un valor de ganancia por
// muestra/bloque; no conoce voces ni el sampler.

#include <memory>

namespace olysf2sampler::dsp {

struct AdsrParameters {
    float attackSeconds{0.0f};
    float decaySeconds{0.0f};
    float sustainLevel{1.0f};
    float releaseSeconds{0.0f};
};

enum class EnvelopeStage {
    Idle,
    Attack,
    Decay,
    Sustain,
    Release,
};

/// Contrato de un generador de envolvente. Realtime-safe por diseño:
/// `nextValue` no asigna memoria ni bloquea.
class Envelope {
public:
    virtual ~Envelope() = default;

    virtual void configure(const AdsrParameters& params) noexcept = 0;
    virtual void noteOn() noexcept = 0;
    virtual void noteOff() noexcept = 0;

    /// Avanza un frame y devuelve la ganancia actual [0.0, 1.0].
    virtual float nextValue(float sampleRateHz) noexcept = 0;

    virtual EnvelopeStage stage() const noexcept = 0;
};

/// Construye la implementación real: ADSR con rampas lineales por
/// etapa, release proporcional al nivel en el momento de noteOff()
/// (para una caída natural, no un salto brusco).
std::unique_ptr<Envelope> createAdsrEnvelope();

}  // namespace olysf2sampler::dsp
