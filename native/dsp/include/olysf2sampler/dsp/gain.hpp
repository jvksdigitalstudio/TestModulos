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

/// Aplica una RAMPA lineal de ganancia (`startGain` -> `endGain`,
/// interpolada muestra a muestra) a `frameCount` muestras in-place.
/// Realtime-safe: sin asignación. Es el bloque de construcción real
/// para smoothing de parámetros (evitar "zipper noise" — Fase A.1
/// §16): quien orquesta la rampa a través de múltiples bloques
/// (SamplerEngine) decide cuántos frames de ESTE bloque corresponden
/// a la rampa y cuáles ya llegaron al target; para esos últimos usar
/// `applyGain` con `endGain` es más barato y equivalente.
void applyGainRamp(float* samples, std::size_t frameCount, float startGain,
                    float endGain) noexcept;

}  // namespace olysf2sampler::dsp
