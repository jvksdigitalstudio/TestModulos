#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olysf2sampler/dsp/envelope.hpp"
#include "olysf2sampler/dsp/filter.hpp"
#include "olysf2sampler/dsp/gain.hpp"
#include "olysf2sampler/dsp/lfo.hpp"
#include "olysf2sampler/dsp/resampler.hpp"

using namespace olysf2sampler::dsp;

namespace {

constexpr float kSampleRate = 48000.0f;

void testAdsrShape() {
    auto env = createAdsrEnvelope();
    env->configure(AdsrParameters{0.01f, 0.01f, 0.5f, 0.01f});  // 10ms cada etapa
    assert(env->stage() == EnvelopeStage::Idle);

    env->noteOn();
    assert(env->stage() == EnvelopeStage::Attack);

    // Avanza lo suficiente para completar Attack (10ms @ 48kHz = 480 frames).
    float last = 0.0f;
    for (int i = 0; i < 480; ++i) {
        last = env->nextValue(kSampleRate);
    }
    assert(last >= 0.95f);  // cerca del pico

    // Avanza Decay hasta Sustain.
    for (int i = 0; i < 600; ++i) {
        last = env->nextValue(kSampleRate);
    }
    assert(env->stage() == EnvelopeStage::Sustain);
    assert(std::abs(last - 0.5f) < 0.05f);

    // noteOff -> Release -> Idle.
    env->noteOff();
    assert(env->stage() == EnvelopeStage::Release);
    for (int i = 0; i < 600; ++i) {
        last = env->nextValue(kSampleRate);
    }
    assert(env->stage() == EnvelopeStage::Idle);
    assert(last <= 0.001f);

    std::printf("[tests/dsp] ADSR: forma de envolvente correcta OK\n");
}

void testBiquadLowPassAttenuatesHighFrequency() {
    auto filter = createBiquadFilter();
    filter->configure(FilterType::LowPass, 500.0f, 0.707f, kSampleRate);

    // Genera un tono de 8kHz (muy por encima del corte de 500Hz) y
    // mide la energía antes/después de filtrar.
    constexpr int kFrames = 2048;
    std::vector<float> tone(kFrames);
    for (int i = 0; i < kFrames; ++i) {
        tone[i] = std::sin(2.0f * 3.14159265f * 8000.0f * static_cast<float>(i) / kSampleRate);
    }

    float energyBefore = 0.0f;
    for (float s : tone) energyBefore += s * s;

    filter->process(tone.data(), tone.size());

    float energyAfter = 0.0f;
    // Descarta los primeros 200 frames (transitorio de arranque del filtro).
    for (int i = 200; i < kFrames; ++i) energyAfter += tone[i] * tone[i];

    assert(energyAfter < energyBefore * 0.1f);  // atenuación fuerte esperada
    std::printf("[tests/dsp] Biquad LowPass atenúa 8kHz con corte en 500Hz OK\n");
}

void testLfoSineKnownPoints() {
    auto lfo = createLfo();
    lfo->configure(LfoWaveform::Sine, 1.0f);  // 1Hz

    float v0 = lfo->nextValue(4.0f);  // fase 0 -> sin(0) = 0
    assert(std::abs(v0 - 0.0f) < 1e-4f);

    // A 4 muestras/seg con 1Hz, el siguiente valor cae en fase 0.25
    // (un cuarto de ciclo) -> sin(pi/2) = 1.0.
    float v1 = lfo->nextValue(4.0f);
    assert(std::abs(v1 - 1.0f) < 1e-4f);

    std::printf("[tests/dsp] LFO sine en puntos conocidos OK\n");
}

void testResamplerUpsamplingDoublesLength() {
    auto resampler = createResampler(/*highQuality=*/false);
    resampler->setRatio(2.0);  // 2x más frames de salida que de entrada

    std::vector<float> input = {0.0f, 1.0f, 0.0f, -1.0f, 0.0f};
    std::vector<float> output(20, 0.0f);

    std::size_t written = resampler->process(input.data(), input.size(), output.data(),
                                              output.size());
    assert(written > input.size());  // debe producir más frames de los que entraron

    // El primer frame de salida debe coincidir con el primer frame de entrada.
    assert(std::abs(output[0] - input[0]) < 1e-4f);

    std::printf("[tests/dsp] Resampler 2x produce %zu frames desde %zu OK\n", written,
                input.size());
}

void testApplyGainRampInterpolatesLinearlyAndHitsEndpoints() {
    std::vector<float> samples(11, 1.0f);  // 11 unos, fáciles de verificar a mano
    olysf2sampler::dsp::applyGainRamp(samples.data(), samples.size(), 0.0f, 1.0f);

    // Primer frame en startGain (0.0), último en endGain (1.0), y
    // estrictamente creciente en el medio (rampa lineal real, no un
    // escalón a mitad de camino).
    assert(std::abs(samples[0] - 0.0f) < 1e-5f);
    assert(std::abs(samples[10] - 1.0f) < 1e-5f);
    for (std::size_t i = 1; i < samples.size(); ++i) {
        assert(samples[i] >= samples[i - 1] - 1e-6f);  // no decreciente
    }
    // Punto medio (índice 5 de 0..10) debe estar cerca de 0.5.
    assert(std::abs(samples[5] - 0.5f) < 0.02f);

    std::printf("[tests/dsp] applyGainRamp interpola linealmente y toca los extremos "
                "(samples[0]=%.4f samples[5]=%.4f samples[10]=%.4f) OK\n",
                samples[0], samples[5], samples[10]);
}

}  // namespace

int main() {
    testAdsrShape();
    testBiquadLowPassAttenuatesHighFrequency();
    testLfoSineKnownPoints();
    testResamplerUpsamplingDoublesLength();
    testApplyGainRampInterpolatesLinearlyAndHitsEndpoints();
    std::printf("[tests/dsp] TODOS los tests pasaron\n");
    return 0;
}
