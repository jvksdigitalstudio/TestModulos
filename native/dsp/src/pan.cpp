#include "olysf2sampler/dsp/pan.hpp"

#include <algorithm>
#include <cmath>

namespace olysf2sampler::dsp {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

StereoGain computeConstantPowerPan(float panPosition) noexcept {
    const float clamped = std::clamp(panPosition, -1.0f, 1.0f);
    // Mapea [-1, 1] -> [0, pi/2] y usa seno/coseno para que
    // left^2 + right^2 == 1 en cualquier posición (potencia constante).
    const float theta = (clamped + 1.0f) * (kPi / 4.0f);
    return StereoGain{std::cos(theta), std::sin(theta)};
}

}  // namespace olysf2sampler::dsp
