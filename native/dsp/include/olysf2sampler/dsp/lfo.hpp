#pragma once
// native/dsp/lfo — osciladores de baja frecuencia para modulación.

#include <memory>

namespace olysf2sampler::dsp {

enum class LfoWaveform {
    Sine,
    Triangle,
    Square,
    SawUp,
};

class Lfo {
public:
    virtual ~Lfo() = default;

    virtual void configure(LfoWaveform waveform, float frequencyHz) noexcept = 0;

    /// Devuelve el siguiente valor de modulación en [-1.0, 1.0].
    virtual float nextValue(float sampleRateHz) noexcept = 0;

    virtual void reset() noexcept = 0;
};

/// Construye la implementación real: acumulador de fase, sin
/// asignación ni tablas precomputadas (cálculo directo por muestra).
std::unique_ptr<Lfo> createLfo();

}  // namespace olysf2sampler::dsp
