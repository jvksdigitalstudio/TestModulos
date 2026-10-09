#include "olysf2sampler/dsp/filter.hpp"

#include <algorithm>
#include <cmath>

namespace olysf2sampler::dsp {

namespace {

// Coeficientes RBJ Audio EQ Cookbook (Robert Bristow-Johnson),
// referencia estándar de la industria para biquads de audio.
// https://www.w3.org/TR/audio-eq-cookbook/ (formulas de dominio
// público ampliamente reproducidas; verificar contra la fuente si se
// necesita exactitud bit a bit con otra implementación de referencia).
struct BiquadCoefficients {
    float b0{1.0f}, b1{0.0f}, b2{0.0f};
    float a1{0.0f}, a2{0.0f};  // a0 normalizado a 1.0
};

BiquadCoefficients computeCoefficients(FilterType type, float cutoffHz, float q,
                                       float sampleRateHz) {
    float nyquist = sampleRateHz * 0.5f;
    float clampedCutoff = std::clamp(cutoffHz, 1.0f, nyquist - 1.0f);
    float clampedQ = std::clamp(q, 0.1f, 20.0f);

    float w0 = 2.0f * static_cast<float>(M_PI) * clampedCutoff / sampleRateHz;
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float alpha = sinw0 / (2.0f * clampedQ);

    float a0, a1, a2, b0, b1, b2;
    switch (type) {
        case FilterType::LowPass:
            b0 = (1.0f - cosw0) / 2.0f;
            b1 = 1.0f - cosw0;
            b2 = (1.0f - cosw0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha;
            break;
        case FilterType::HighPass:
            b0 = (1.0f + cosw0) / 2.0f;
            b1 = -(1.0f + cosw0);
            b2 = (1.0f + cosw0) / 2.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha;
            break;
        case FilterType::BandPass:
            b0 = alpha;
            b1 = 0.0f;
            b2 = -alpha;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha;
            break;
        case FilterType::Notch:
        default:
            b0 = 1.0f;
            b1 = -2.0f * cosw0;
            b2 = 1.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosw0;
            a2 = 1.0f - alpha;
            break;
    }

    BiquadCoefficients c;
    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

class BiquadFilterImpl final : public Filter {
public:
    void configure(FilterType type, float cutoffHz, float resonance,
                   float sampleRateHz) noexcept override {
        if (sampleRateHz <= 0.0f) {
            return;
        }
        coeffs_ = computeCoefficients(type, cutoffHz, resonance, sampleRateHz);
    }

    void process(float* samples, std::size_t frameCount) noexcept override {
        if (samples == nullptr) {
            return;
        }
        for (std::size_t i = 0; i < frameCount; ++i) {
            float x0 = samples[i];
            float y0 = coeffs_.b0 * x0 + coeffs_.b1 * x1_ + coeffs_.b2 * x2_ -
                       coeffs_.a1 * y1_ - coeffs_.a2 * y2_;
            x2_ = x1_;
            x1_ = x0;
            y2_ = y1_;
            y1_ = y0;
            samples[i] = y0;
        }
    }

    void reset() noexcept override { x1_ = x2_ = y1_ = y2_ = 0.0f; }

private:
    BiquadCoefficients coeffs_;
    float x1_{0.0f}, x2_{0.0f}, y1_{0.0f}, y2_{0.0f};  // memoria Direct Form I
};

}  // namespace

std::unique_ptr<Filter> createBiquadFilter() { return std::make_unique<BiquadFilterImpl>(); }

}  // namespace olysf2sampler::dsp
