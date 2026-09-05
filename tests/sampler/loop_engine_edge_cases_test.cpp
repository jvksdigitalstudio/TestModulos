// tests/sampler/loop_engine_edge_cases_test.cpp
//
// Fase A.1 §14: verifica (no corrige — ya estaba bien) que el loop
// engine de Voice maneja con seguridad los casos límite explícitos
// del prompt: loopLength == 0 y loopStart >= loopEnd. En ambos casos
// la condición de guarda en voice_impl.cpp (`loopEndFrame >
// loopStartFrame`) hace que la voz simplemente termine de reproducir
// el sample sin loop, en vez de dividir por cero (std::fmod con
// divisor 0) o dejar `position_` en un estado inconsistente.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olysf2sampler/sampler/sampler_engine.hpp"
#include "olysf2sampler/sampler/mapping.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;

// Reproduce una nota con un SampleSource de loop inválido y confirma
// que el motor produce audio y luego decae a silencio limpiamente
// (sin loop infinito, sin NaN/Inf, sin crash) en vez de colgarse o
// corromper la posición de lectura.
void testInvalidLoopFallsBackToNonLoopingPlayback(std::uint32_t loopStart,
                                                   std::uint32_t loopEnd,
                                                   const char* caseName) {
    std::vector<std::int16_t> pcm(2000, 8000);  // sample corto, sin silencio final

    auto engine = createSamplerEngine(kSampleRate);
    auto mapping = createKeyVelocityMapping();
    mapping->setZones({MappingZone{0, 127, 0, 127, /*sampleId=*/1}});
    engine->setMapping(mapping.get());

    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    source.loopEnabled = true;
    source.loopStartFrame = loopStart;
    source.loopEndFrame = loopEnd;  // inválido a propósito
    engine->loadSample(1, source);

    engine->noteOn(60, 100);

    // Render varios bloques, bien más allá de la duración del sample
    // (2000 frames a 48kHz ~ 42ms). Si hubiera un bug de división por
    // cero o loop infinito de posición, esto produciría NaN/Inf o
    // simplemente nunca decaería.
    std::vector<float> buffer(4096, 0.0f);
    bool sawFiniteAudio = false;
    for (int block = 0; block < 20; ++block) {
        engine->renderBlock(buffer.data(), buffer.size());
        for (float s : buffer) {
            assert(std::isfinite(s));  // nunca NaN/Inf
            if (s != 0.0f) sawFiniteAudio = true;
        }
    }
    assert(sawFiniteAudio);  // sonó algo antes de apagarse

    float tail = std::abs(buffer.back());
    assert(tail < 0.01f);  // terminó en silencio (no loop infinito), caso: caseName

    std::printf("[tests/sampler] loop inválido (%s) degrada a reproducción sin loop, sin NaN/Inf, "
                "termina en silencio OK\n",
                caseName);
}

}  // namespace

int main() {
    testInvalidLoopFallsBackToNonLoopingPlayback(100, 100, "loopLength == 0");
    testInvalidLoopFallsBackToNonLoopingPlayback(200, 100, "loopStart >= loopEnd");
    std::printf("[tests/sampler] loop_engine_edge_cases_test: TODOS los tests pasaron\n");
    return 0;
}
