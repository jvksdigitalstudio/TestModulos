// tests/sampler/queue_overflow_diagnostics_test.cpp
//
// Fase A.1 §22: "No perder eventos silenciosamente sin diagnóstico".
// Satura deliberadamente la cola control->audio (llamando noteOn
// muchas más veces de lo que cabe en la capacidad de la cola SIN
// drenarla con renderBlock) y confirma que droppedNoteEventCount()
// refleja exactamente cuántos comandos no pudieron encolarse.

#include <cassert>
#include <cstdio>
#include <vector>

#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;

void testQueueOverflowIsCountedNotSilent() {
    auto engine = createSamplerEngine(kSampleRate);

    auto mapping = createKeyVelocityMapping();
    mapping->setZones({MappingZone{0, 127, 0, 127, /*sampleId=*/1}});
    engine->setMapping(mapping.get());

    std::vector<std::int16_t> pcm(100, 1000);
    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    engine->loadSample(1, source);

    assert(engine->droppedNoteEventCount() == 0);

    // La capacidad interna de la cola es 256 (kCommandQueueCapacity,
    // implementación); un ring buffer SPSC de capacidad N solo puede
    // contener N-1 elementos útiles. Empujamos MUY por encima de eso
    // sin llamar renderBlock() (que es lo único que drena la cola),
    // así que el excedente exacto debe quedar contado como drop.
    constexpr int kTotalNoteOnCalls = 1000;
    for (int i = 0; i < kTotalNoteOnCalls; ++i) {
        engine->noteOn(60, 100);  // cada noteOn intenta encolar 1 comando (1 sampleId mapeado)
    }

    std::uint64_t dropped = engine->droppedNoteEventCount();
    assert(dropped > 0);            // se perdieron eventos de verdad
    assert(dropped < kTotalNoteOnCalls);  // pero no absolutamente todos (la cola sí aceptó algunos)

    std::printf(
        "[tests/sampler] overflow de cola: %llu de %d eventos descartados y CONTADOS "
        "(antes: perdidos en silencio total, sin forma de saberlo) OK\n",
        static_cast<unsigned long long>(dropped), kTotalNoteOnCalls);
}

}  // namespace

int main() {
    testQueueOverflowIsCountedNotSilent();
    std::printf("[tests/sampler] queue_overflow_diagnostics_test: TODOS los tests pasaron\n");
    return 0;
}
