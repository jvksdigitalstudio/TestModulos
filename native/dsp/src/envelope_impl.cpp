#include "olysf2sampler/dsp/envelope.hpp"

#include <algorithm>

namespace olysf2sampler::dsp {

namespace {

class AdsrEnvelopeImpl final : public Envelope {
public:
    void configure(const AdsrParameters& params) noexcept override { params_ = params; }

    void noteOn() noexcept override {
        stage_ = EnvelopeStage::Attack;
        // Se retoma desde el nivel actual (no se fuerza a 0), para
        // evitar un salto audible si llega un noteOn mientras una
        // voz anterior seguía en Release (retrigger).
    }

    void noteOff() noexcept override {
        if (stage_ == EnvelopeStage::Idle) {
            return;
        }
        releaseStartLevel_ = level_;
        stage_ = EnvelopeStage::Release;
    }

    float nextValue(float sampleRateHz) noexcept override {
        if (sampleRateHz <= 0.0f) {
            return level_;
        }
        switch (stage_) {
            case EnvelopeStage::Idle:
                level_ = 0.0f;
                break;

            case EnvelopeStage::Attack: {
                float increment = params_.attackSeconds > 0.0f
                                       ? 1.0f / (params_.attackSeconds * sampleRateHz)
                                       : 1.0f;
                level_ += increment;
                if (level_ >= 1.0f) {
                    level_ = 1.0f;
                    stage_ = EnvelopeStage::Decay;
                }
                break;
            }

            case EnvelopeStage::Decay: {
                float target = std::clamp(params_.sustainLevel, 0.0f, 1.0f);
                float increment = params_.decaySeconds > 0.0f
                                       ? (1.0f - target) / (params_.decaySeconds * sampleRateHz)
                                       : 1.0f;
                level_ -= increment;
                if (level_ <= target) {
                    level_ = target;
                    stage_ = EnvelopeStage::Sustain;
                }
                break;
            }

            case EnvelopeStage::Sustain:
                level_ = std::clamp(params_.sustainLevel, 0.0f, 1.0f);
                break;

            case EnvelopeStage::Release: {
                float increment = params_.releaseSeconds > 0.0f
                                       ? releaseStartLevel_ / (params_.releaseSeconds * sampleRateHz)
                                       : releaseStartLevel_;
                level_ -= increment;
                if (level_ <= 0.0f) {
                    level_ = 0.0f;
                    stage_ = EnvelopeStage::Idle;
                }
                break;
            }
        }
        return level_;
    }

    EnvelopeStage stage() const noexcept override { return stage_; }

private:
    AdsrParameters params_;
    EnvelopeStage stage_{EnvelopeStage::Idle};
    float level_{0.0f};
    float releaseStartLevel_{0.0f};
};

}  // namespace

std::unique_ptr<Envelope> createAdsrEnvelope() { return std::make_unique<AdsrEnvelopeImpl>(); }

}  // namespace olysf2sampler::dsp
