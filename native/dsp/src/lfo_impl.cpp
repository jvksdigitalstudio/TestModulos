#include "olysf2sampler/dsp/lfo.hpp"

#include <cmath>

namespace olysf2sampler::dsp {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

class LfoImpl final : public Lfo {
public:
    void configure(LfoWaveform waveform, float frequencyHz) noexcept override {
        waveform_ = waveform;
        frequencyHz_ = frequencyHz;
    }

    float nextValue(float sampleRateHz) noexcept override {
        if (sampleRateHz <= 0.0f) {
            return 0.0f;
        }
        float value = evaluate(phase_);

        phase_ += frequencyHz_ / sampleRateHz;
        if (phase_ >= 1.0f) {
            phase_ -= std::floor(phase_);
        }
        return value;
    }

    void reset() noexcept override { phase_ = 0.0f; }

private:
    /// `phase` en [0.0, 1.0). Devuelve el valor de la forma de onda en
    /// [-1.0, 1.0] para esa fase.
    float evaluate(float phase) const noexcept {
        switch (waveform_) {
            case LfoWaveform::Sine:
                return std::sin(kTwoPi * phase);
            case LfoWaveform::Triangle:
                // Triángulo: sube de -1 a 1 en la primera mitad del
                // ciclo, baja de 1 a -1 en la segunda.
                return phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
            case LfoWaveform::Square:
                return phase < 0.5f ? 1.0f : -1.0f;
            case LfoWaveform::SawUp:
                return 2.0f * phase - 1.0f;
        }
        return 0.0f;
    }

    LfoWaveform waveform_{LfoWaveform::Sine};
    float frequencyHz_{1.0f};
    float phase_{0.0f};
};

}  // namespace

std::unique_ptr<Lfo> createLfo() { return std::make_unique<LfoImpl>(); }

}  // namespace olysf2sampler::dsp
