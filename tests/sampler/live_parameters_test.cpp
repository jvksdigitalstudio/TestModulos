#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;

std::vector<std::int16_t> makeTwoToneWaveform(std::size_t frameCount) {
    std::vector<std::int16_t> data(frameCount);
    for (std::size_t i = 0; i < frameCount; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        double low = std::sin(2.0 * 3.14159265358979 * 200.0 * t);
        double high = std::sin(2.0 * 3.14159265358979 * 10000.0 * t);
        data[i] = static_cast<std::int16_t>(8000.0 * (0.5 * low + 0.5 * high));
    }
    return data;
}

double peakAbs(const std::vector<float>& buffer) {
    double peak = 0.0;
    for (float s : buffer) peak = std::max(peak, static_cast<double>(std::abs(s)));
    return peak;
}

double roughness(const std::vector<float>& buffer) {
    double sum = 0.0;
    for (std::size_t i = 1; i < buffer.size(); ++i) {
        sum += std::abs(static_cast<double>(buffer[i]) - static_cast<double>(buffer[i - 1]));
    }
    return sum;
}

std::unique_ptr<SamplerEngine> buildEngineWithTestSample(
    std::vector<std::int16_t>& pcmStorage, std::unique_ptr<KeyVelocityMapping>& mappingStorage) {
    auto engine = createSamplerEngine(kSampleRate);
    mappingStorage = createKeyVelocityMapping();
    mappingStorage->setZones({MappingZone{0, 127, 0, 127, 1}});
    engine->setMapping(mappingStorage.get());

    SampleSource source;
    source.pcmData = pcmStorage.data();
    source.frameCount = pcmStorage.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    source.loopEnabled = true;
    source.loopStartFrame = 0;
    source.loopEndFrame = static_cast<std::uint32_t>(pcmStorage.size());
    engine->loadSample(1, source);
    return engine;
}

void testMasterVolumeScalesOutput() {
    auto pcm = makeTwoToneWaveform(4000);
    std::unique_ptr<KeyVelocityMapping> mapping;
    auto engine = buildEngineWithTestSample(pcm, mapping);

    engine->noteOn(60, 100);

    std::vector<float> fullVolume(512, 0.0f);
    engine->renderBlock(fullVolume.data(), fullVolume.size());  // drena el noteOn + 1er bloque a vol=1.0
    double peakFull = peakAbs(fullVolume);
    assert(peakFull > 0.05);

    engine->setMasterVolume(0.25f);
    std::vector<float> quietVolume(512, 0.0f);
    engine->renderBlock(quietVolume.data(), quietVolume.size());
    double peakQuiet = peakAbs(quietVolume);

    // Debe ser notablemente más bajo (permite margen por la envolvente
    // seguir subiendo/bajando entre bloques, no una igualdad exacta).
    assert(peakQuiet < peakFull * 0.6);
    std::printf("[tests/sampler] setMasterVolume(0.25) reduce el pico real (%.4f -> %.4f) OK\n",
               peakFull, peakQuiet);
}

void testFilterCutoffAttenuatesHighFrequency() {
    auto pcm = makeTwoToneWaveform(8000);
    std::unique_ptr<KeyVelocityMapping> mapping;
    auto engine = buildEngineWithTestSample(pcm, mapping);

    engine->noteOn(60, 100);

    std::vector<float> unfiltered(4000, 0.0f);
    engine->renderBlock(unfiltered.data(), unfiltered.size());
    double roughnessBefore = roughness(unfiltered);

    engine->setFilterCutoff(500.0f, 0.707f);
    // Deja que el cambio de filtro se aplique (ocurre al inicio del
    // siguiente renderBlock) y que el biquad asiente su transitorio.
    std::vector<float> settling(1000, 0.0f);
    engine->renderBlock(settling.data(), settling.size());

    std::vector<float> filtered(4000, 0.0f);
    engine->renderBlock(filtered.data(), filtered.size());
    double roughnessAfter = roughness(filtered);

    assert(roughnessAfter < roughnessBefore * 0.5);
    std::printf(
        "[tests/sampler] setFilterCutoff(500Hz) atenúa agudos de verdad (rugosidad %.1f -> "
        "%.1f) OK\n",
        roughnessBefore, roughnessAfter);
}

}  // namespace

int main() {
    testMasterVolumeScalesOutput();
    testFilterCutoffAttenuatesHighFrequency();
    std::printf("[tests/sampler] parámetros en tiempo real: todos los tests pasaron\n");
    return 0;
}
