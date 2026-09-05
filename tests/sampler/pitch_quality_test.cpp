// tests/sampler/pitch_quality_test.cpp
//
// Fase A.1 §38 ("no crear interfaces vacías") + §21 (interpolación no
// debe estar acoplada/hardcodeada dentro de Voice): `PitchQuality`
// existía declarado en voice.hpp sin que NADA lo consumiera —
// Voice::renderInto tenía dsp::InterpolationMethod::Cubic como
// literal fijo. Este test prueba, con audio real, que
// SamplerEngine::setPitchQuality llega de verdad hasta el algoritmo
// de interpolación (Fast=Linear vs HighQuality=Cubic producen salidas
// medible y consistentemente distintas para una nota con pitch
// fraccional), no que simplemente compila.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;

// Sample con mucho contenido armónico (diente de sierra aproximado)
// para que la diferencia entre interpolación lineal y cúbica sea
// medible: cerca de discontinuidades, cúbica y lineal divergen más.
std::vector<std::int16_t> makeSawtoothWaveform(std::size_t frameCount) {
    std::vector<std::int16_t> data(frameCount);
    constexpr double freq = 440.0;
    for (std::size_t i = 0; i < frameCount; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        double phase = std::fmod(freq * t, 1.0);
        double saw = 2.0 * phase - 1.0;  // diente de sierra en [-1, 1)
        data[i] = static_cast<std::int16_t>(12000.0 * saw);
    }
    return data;
}

std::vector<float> renderWithQuality(PitchQuality quality) {
    std::vector<std::int16_t> pcm = makeSawtoothWaveform(4000);

    auto engine = createSamplerEngine(kSampleRate);
    auto mapping = createKeyVelocityMapping();
    mapping->setZones({MappingZone{0, 127, 0, 127, /*sampleId=*/1}});
    engine->setMapping(mapping.get());

    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    engine->loadSample(1, source);

    engine->setPitchQuality(quality);

    // Nota bien alejada de rootNote=60 para forzar un ratio de
    // resampling claramente no-entero (pitch fraccional real), que es
    // donde lineal y cúbica más divergen.
    engine->noteOn(79, 100);  // +19 semitonos ~ ratio != 1.0 claro

    std::vector<float> buffer(2048, 0.0f);
    engine->renderBlock(buffer.data(), buffer.size());
    return buffer;
}

void testPitchQualityIsWiredThroughToRealAudio() {
    std::vector<float> fast = renderWithQuality(PitchQuality::Fast);
    std::vector<float> highQuality = renderWithQuality(PitchQuality::HighQuality);

    assert(fast.size() == highQuality.size());

    double maxAbsDiff = 0.0;
    double sumAbsFast = 0.0;
    for (std::size_t i = 0; i < fast.size(); ++i) {
        maxAbsDiff = std::max(maxAbsDiff, std::abs(static_cast<double>(fast[i]) -
                                                     static_cast<double>(highQuality[i])));
        sumAbsFast += std::abs(fast[i]);
    }

    assert(sumAbsFast > 0.0);   // ambas produjeron audio real, no silencio
    // Si setPitchQuality NO estuviera realmente conectado (el bug que
    // este test existe para prevenir), fast == highQuality bit a bit
    // porque ambos usarían el mismo Cubic hardcodeado. Una diferencia
    // medible confirma que el parámetro realmente cambia el algoritmo.
    assert(maxAbsDiff > 1e-4);

    std::printf(
        "[tests/sampler] setPitchQuality(Fast) vs setPitchQuality(HighQuality) producen audio "
        "medible y distinto (diff máx %.5f) — antes: enum declarado pero sin ningún consumidor "
        "OK\n",
        maxAbsDiff);
}

}  // namespace

int main() {
    testPitchQualityIsWiredThroughToRealAudio();
    std::printf("[tests/sampler] pitch_quality_test: TODOS los tests pasaron\n");
    return 0;
}
