#include "olysf2sampler/dsp/gain.hpp"

#include <cmath>

namespace olysf2sampler::dsp {

float decibelsToLinear(float decibels) noexcept {
    return std::pow(10.0f, decibels / 20.0f);
}

float linearToDecibels(float linear) noexcept {
    constexpr float kSilenceFloor = 1e-9f;
    return 20.0f * std::log10(linear > kSilenceFloor ? linear : kSilenceFloor);
}

void applyGain(float* samples, std::size_t frameCount, float linearGain) noexcept {
    if (samples == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < frameCount; ++i) {
        samples[i] *= linearGain;
    }
}

void applyGainRamp(float* samples, std::size_t frameCount, float startGain,
                    float endGain) noexcept {
    if (samples == nullptr || frameCount == 0) {
        return;
    }
    if (frameCount == 1) {
        // Un único frame: no hay "entre muestras" que interpolar,
        // aplicar directamente el gain final evita división por cero
        // en (frameCount - 1) más abajo.
        samples[0] *= endGain;
        return;
    }
    float step = (endGain - startGain) / static_cast<float>(frameCount - 1);
    float gain = startGain;
    for (std::size_t i = 0; i < frameCount; ++i) {
        samples[i] *= gain;
        gain += step;
    }
}

}  // namespace olysf2sampler::dsp
