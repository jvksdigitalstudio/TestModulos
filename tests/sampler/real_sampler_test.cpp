#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;

// Genera una "onda" de prueba de 1000 frames (no es audio real, es un
// diente de sierra reconocible para verificar que el pitch/loop se
// comportan de forma predecible).
std::vector<std::int16_t> makeTestWaveform(std::size_t frameCount) {
    std::vector<std::int16_t> data(frameCount);
    for (std::size_t i = 0; i < frameCount; ++i) {
        data[i] = static_cast<std::int16_t>(
            10000.0 * std::sin(2.0 * 3.14159265358979 * 4.0 * static_cast<double>(i) /
                               static_cast<double>(frameCount)));
    }
    return data;
}

void testNoteOnProducesNonSilentAudio() {
    std::vector<std::int16_t> pcm = makeTestWaveform(2000);

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

    engine->noteOn(60, 100);  // misma nota que rootNote -> ratio de pitch 1:1

    std::vector<float> buffer(512, 0.0f);
    engine->renderBlock(buffer.data(), buffer.size());

    bool hasNonSilentSample = false;
    for (float s : buffer) {
        if (std::abs(s) > 1e-4f) {
            hasNonSilentSample = true;
            break;
        }
    }
    assert(hasNonSilentSample);
    std::printf("[tests/sampler] noteOn produce audio no-silencioso OK\n");
}

void testNoteOffTriggersReleaseFadeToSilence() {
    std::vector<std::int16_t> pcm(4000, 10000);  // señal constante, fácil de verificar decaimiento

    auto engine = createSamplerEngine(kSampleRate);
    auto mapping = createKeyVelocityMapping();
    mapping->setZones({MappingZone{0, 127, 0, 127, 1}});
    engine->setMapping(mapping.get());

    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    source.loopEnabled = true;
    source.loopStartFrame = 0;
    source.loopEndFrame = static_cast<std::uint32_t>(pcm.size());
    engine->loadSample(1, source);

    engine->noteOn(60, 100);

    std::vector<float> buffer(4096, 0.0f);  // suficiente para pasar Attack+Decay -> Sustain
    engine->renderBlock(buffer.data(), buffer.size());
    float sustainLevel = std::abs(buffer.back());
    assert(sustainLevel > 0.1f);  // debe seguir sonando (loop activo, en Sustain)

    engine->noteOff(60);

    // 0.15s de release @ 48kHz = 7200 frames; usamos más margen.
    std::vector<float> releaseBuffer(10000, 0.0f);
    engine->renderBlock(releaseBuffer.data(), releaseBuffer.size());
    float tailLevel = std::abs(releaseBuffer.back());
    assert(tailLevel < 0.01f);  // debe haber decaído a (casi) silencio

    std::printf("[tests/sampler] noteOff decae a silencio tras release OK\n");
}

void testPolyphonyAndVoiceStealing() {
    std::vector<std::int16_t> pcm = makeTestWaveform(2000);

    auto engine = createSamplerEngine(kSampleRate);
    auto mapping = createKeyVelocityMapping();
    // Una zona por nota MIDI distinta, todas apuntando al mismo sample.
    std::vector<MappingZone> zones;
    for (int note = 20; note < 20 + 40; ++note) {
        zones.push_back(MappingZone{note, note, 0, 127, 1});
    }
    mapping->setZones(zones);
    engine->setMapping(mapping.get());

    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    engine->loadSample(1, source);

    // Dispara 40 notas distintas — más que la polifonía por defecto
    // (32) — para forzar voice-stealing. No debe crashear ni bloquear.
    for (int note = 20; note < 60; ++note) {
        engine->noteOn(note, 100);
    }

    std::vector<float> buffer(512, 0.0f);
    engine->renderBlock(buffer.data(), buffer.size());  // no debe crashear

    bool hasNonSilentSample = false;
    for (float s : buffer) {
        if (std::abs(s) > 1e-4f) {
            hasNonSilentSample = true;
            break;
        }
    }
    assert(hasNonSilentSample);
    std::printf("[tests/sampler] 40 notas simultáneas (voice-stealing) sin crash OK\n");
}

void testMappingRespectsKeyRange() {
    auto mapping = createKeyVelocityMapping();
    mapping->setZones({
        MappingZone{0, 59, 0, 127, /*sampleId=*/100},
        MappingZone{60, 127, 0, 127, /*sampleId=*/200},
    });

    auto low = mapping->resolve(40, 100);
    assert(low.size() == 1 && low[0] == 100);

    auto high = mapping->resolve(90, 100);
    assert(high.size() == 1 && high[0] == 200);

    std::printf("[tests/sampler] KeyVelocityMapping respeta rangos de nota OK\n");
}

}  // namespace

int main() {
    testNoteOnProducesNonSilentAudio();
    testNoteOffTriggersReleaseFadeToSilence();
    testPolyphonyAndVoiceStealing();
    testMappingRespectsKeyRange();
    std::printf("[tests/sampler] TODOS los tests pasaron\n");
    return 0;
}
