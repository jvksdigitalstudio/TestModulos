#include "olysf2sampler/sampler/voice.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "olysf2sampler/dsp/envelope.hpp"
#include "olysf2sampler/dsp/interpolation.hpp"

namespace olysf2sampler::sampler {

namespace {

// Envolvente por defecto fija (click-free, sin decaimiento agresivo).
// La decodificacion de generadores SF2 hacia parametros reales de
// envolvente es una fase posterior (ver voice.hpp).
constexpr dsp::AdsrParameters kDefaultEnvelope{
    /*attackSeconds=*/0.005f,
    /*decaySeconds=*/0.05f,
    /*sustainLevel=*/0.8f,
    /*releaseSeconds=*/0.15f,
};

// Tamano maximo de bloque que el filtro por voz puede procesar sin
// asignar memoria (buffer de scratch pre-reservado como miembro fijo
// de VoiceImpl, no en el stack de renderBlock ni en el heap). 4096
// frames es generoso para cualquier tamano de bloque de audio real
// (los callbacks tipicos son de 64 a 1024 frames).
constexpr std::size_t kMaxFilterScratchFrames = 4096;

float semitoneRatio(float semitonesAndCents) {
    return std::pow(2.0f, semitonesAndCents / 1200.0f);
}

class VoiceImpl final : public Voice {
public:
    explicit VoiceImpl(float engineSampleRateHz)
        : engineSampleRateHz_(engineSampleRateHz),
          envelope_(dsp::createAdsrEnvelope()),
          filter_(dsp::createBiquadFilter()) {
        envelope_->configure(kDefaultEnvelope);
    }

    void trigger(const NoteEvent& event, const SampleSource& source) noexcept override {
        event_ = event;
        source_ = source;
        position_ = 0.0;

        float totalCents = static_cast<float>(event.midiNote - source.rootNote) * 100.0f +
                           event.fineTuneCents + static_cast<float>(source.pitchCorrectionCents);
        float pitchRatio = semitoneRatio(totalCents);
        float sampleRateRatio =
            source.sampleRateHz > 0 ? source.sampleRateHz / engineSampleRateHz_ : 1.0f;
        readStep_ = static_cast<double>(pitchRatio) * static_cast<double>(sampleRateRatio);
        if (readStep_ <= 0.0) {
            readStep_ = 1.0;
        }

        envelope_->configure(kDefaultEnvelope);
        envelope_->noteOn();
        if (filterEnabled_) {
            filter_->reset();
        }
        state_ = VoiceState::Playing;
    }

    void release() noexcept override {
        if (state_ == VoiceState::Idle) {
            return;
        }
        envelope_->noteOff();
        state_ = VoiceState::Releasing;
    }

    void setFilterCutoff(dsp::FilterType type, float cutoffHz, float resonance) noexcept override {
        filter_->configure(type, cutoffHz, resonance, engineSampleRateHz_);
        filterEnabled_ = true;
    }

    void disableFilter() noexcept override { filterEnabled_ = false; }

    void renderBlock(float* outputBuffer, std::size_t frameCount) noexcept override {
        if (state_ == VoiceState::Idle || outputBuffer == nullptr ||
            source_.pcmData == nullptr || source_.frameCount == 0) {
            return;
        }

        if (!filterEnabled_) {
            renderInto(outputBuffer, frameCount, /*accumulate=*/true);
            return;
        }

        // Con filtro: renderiza SIN acumular a un scratch pre-asignado
        // (sin heap alloc), filtra ese scratch in-place, y recien ahi
        // acumula al buffer de salida real.
        std::size_t chunk = std::min(frameCount, kMaxFilterScratchFrames);
        std::fill(scratch_.begin(), scratch_.begin() + static_cast<long>(chunk), 0.0f);
        renderInto(scratch_.data(), chunk, /*accumulate=*/false);
        filter_->process(scratch_.data(), chunk);
        for (std::size_t i = 0; i < chunk; ++i) {
            outputBuffer[i] += scratch_[i];
        }
        // Si frameCount > kMaxFilterScratchFrames (caso extremo no
        // realista en audio real-time), el resto del bloque se
        // renderiza sin filtro en vez de fallar silenciosamente sin
        // sonido -- degradacion documentada, no corrupcion.
        if (frameCount > kMaxFilterScratchFrames && state_ != VoiceState::Idle) {
            renderInto(outputBuffer + kMaxFilterScratchFrames, frameCount - kMaxFilterScratchFrames,
                      /*accumulate=*/true);
        }
    }

    VoiceState state() const noexcept override { return state_; }

private:
    void renderInto(float* buffer, std::size_t frameCount, bool accumulate) noexcept {
        for (std::size_t i = 0; i < frameCount; ++i) {
            if (state_ == VoiceState::Idle) {
                break;
            }

            std::int64_t idx = static_cast<std::int64_t>(position_);
            if (idx >= static_cast<std::int64_t>(source_.frameCount)) {
                if (source_.loopEnabled && source_.loopEndFrame > source_.loopStartFrame) {
                    double loopLength =
                        static_cast<double>(source_.loopEndFrame - source_.loopStartFrame);
                    position_ = static_cast<double>(source_.loopStartFrame) +
                               std::fmod(position_ - static_cast<double>(source_.loopStartFrame),
                                         loopLength);
                    idx = static_cast<std::int64_t>(position_);
                } else {
                    state_ = VoiceState::Idle;
                    break;
                }
            }

            float frac = static_cast<float>(position_ - static_cast<double>(idx));
            float y0 = sampleAt(idx - 1);
            float y1 = sampleAt(idx);
            float y2 = sampleAt(idx + 1);
            float y3 = sampleAt(idx + 2);
            float raw =
                dsp::interpolateSample(dsp::InterpolationMethod::Cubic, y0, y1, y2, y3, frac);

            float gain = envelope_->nextValue(engineSampleRateHz_);
            float sample = raw * gain;
            if (accumulate) {
                buffer[i] += sample;
            } else {
                buffer[i] = sample;
            }

            if (envelope_->stage() == dsp::EnvelopeStage::Idle &&
                state_ == VoiceState::Releasing) {
                state_ = VoiceState::Idle;
            }

            position_ += readStep_;
        }
    }

    float sampleAt(std::int64_t idx) const noexcept {
        if (idx < 0 || idx >= static_cast<std::int64_t>(source_.frameCount)) {
            // Fuera de rango del sample: silencio, nunca lectura fuera
            // de limites (source_.pcmData tiene exactamente
            // source_.frameCount frames validos).
            return 0.0f;
        }
        return static_cast<float>(source_.pcmData[idx]) / 32768.0f;
    }

    float engineSampleRateHz_;
    std::unique_ptr<dsp::Envelope> envelope_;
    std::unique_ptr<dsp::Filter> filter_;
    bool filterEnabled_{false};
    std::array<float, kMaxFilterScratchFrames> scratch_{};

    NoteEvent event_;
    SampleSource source_;
    VoiceState state_{VoiceState::Idle};
    double position_{0.0};
    double readStep_{1.0};
};

}  // namespace

std::unique_ptr<Voice> createVoice(float engineSampleRateHz) {
    return std::make_unique<VoiceImpl>(engineSampleRateHz);
}

}  // namespace olysf2sampler::sampler
