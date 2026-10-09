#pragma once
// native/dsp/pan — posicionamiento estéreo (constant-power panning).
// Responsabilidad única: distribuir una señal mono entre canales L/R
// (o ajustar el balance de una señal ya estéreo). No conoce voces ni
// mezcla de múltiples fuentes.

namespace olysf2sampler::dsp {

struct StereoGain {
    float left{1.0f};
    float right{1.0f};
};

/// Calcula las ganancias L/R para una posición de pan en [-1.0, 1.0]
/// (-1 = izquierda total, 0 = centro, +1 = derecha total) usando la
/// ley de paneo de potencia constante (constant-power law), que
/// mantiene percibida la misma energía total en cualquier posición.
StereoGain computeConstantPowerPan(float panPosition) noexcept;

}  // namespace olysf2sampler::dsp
