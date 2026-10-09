#pragma once
// native/dsp/interpolation — interpolación de muestras individuales
// (distinta de resampling de buffer completo: esto es la primitiva de
// bajo nivel que un resampler o un lector de sample con pitch
// fraccional usa internamente para calcular un valor entre dos
// muestras consecutivas). Responsabilidad única: matemática pura,
// sin estado, sin buffers.

namespace olysf2sampler::dsp {

enum class InterpolationMethod {
    None,        // nearest-neighbor
    Linear,
    Cubic,
};

/// Interpola entre `y0`..`y3` (4 puntos, para permitir cúbica) en la
/// fracción `frac` [0.0, 1.0) entre `y1` e `y2`. Los métodos que no
/// necesitan 4 puntos (None, Linear) ignoran los que sobran.
/// Pura función, realtime-safe por construcción (sin estado, sin
/// asignación).
float interpolateSample(InterpolationMethod method, float y0, float y1, float y2, float y3,
                         float frac) noexcept;

}  // namespace olysf2sampler::dsp
