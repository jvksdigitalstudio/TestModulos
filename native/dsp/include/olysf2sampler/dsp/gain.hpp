#pragma once
// native/dsp/gain — control de ganancia/volumen. Responsabilidad
// única: aplicar un factor de ganancia (lineal o en dB) a un buffer.
// No decide de dónde viene la ganancia (eso lo resuelve
// sampler::Modulator combinando envolvente + velocity + etc.).

#include <cstddef>

namespace olysf2sampler::dsp {

float decibelsToLinear(float decibels) noexcept;
float linearToDecibels(float linear) noexcept;

/// Aplica una ganancia lineal constante a `frameCount` muestras
/// in-place. Realtime-safe: sin asignación.
void applyGain(float* samples, std::size_t frameCount, float linearGain) noexcept;

}  // namespace olysf2sampler::dsp
