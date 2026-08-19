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

}  // namespace olysf2sampler::dsp
