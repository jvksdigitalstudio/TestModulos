#include "olysf2sampler/dsp/interpolation.hpp"

namespace olysf2sampler::dsp {

namespace {

float linear(float y1, float y2, float frac) {
    return y1 + (y2 - y1) * frac;
}

// Interpolación cúbica de Hermite (Catmull-Rom), estándar para
// resampling de audio de buena calidad con costo moderado.
float cubic(float y0, float y1, float y2, float y3, float frac) {
    const float a0 = y3 - y2 - y0 + y1;
    const float a1 = y0 - y1 - a0;
    const float a2 = y2 - y0;
    const float a3 = y1;
    return a0 * frac * frac * frac + a1 * frac * frac + a2 * frac + a3;
}

}  // namespace

float interpolateSample(InterpolationMethod method, float y0, float y1, float y2, float y3,
                         float frac) noexcept {
    switch (method) {
        case InterpolationMethod::None:
            return frac < 0.5f ? y1 : y2;
        case InterpolationMethod::Linear:
            return linear(y1, y2, frac);
        case InterpolationMethod::Cubic:
            return cubic(y0, y1, y2, y3, frac);
    }
    return y1;
}

}  // namespace olysf2sampler::dsp
