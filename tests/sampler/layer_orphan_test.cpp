// tests/sampler/layer_orphan_test.cpp
//
// Regresión Fase A.1 §13: antes de este fix, `kMaxLayersPerNote` era
// una constante fija (8) independiente de la polifonía real del
// motor. Un preset con más de 8 capas superpuestas en una misma nota
// dejaba las capas "de más" sonando sin registrar: `noteOff` sobre
// esa nota no las apagaba, y quedaban huérfanas hasta que
// voice-stealing las reclamara para otra nota (más tarde, sin
// relación con la intención del usuario). Con el fix, el límite de
// capas rastreables por nota se dimensiona a la polifonía máxima
// configurada del motor, así que esto es estructuralmente imposible:
// nunca puede haber más voces asignadas a una nota que el tamaño
// total del pool.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;

void testManyLayersOnOneNoteAllReleaseOnNoteOff() {
    // Señal constante en loop: fácil de distinguir "sigue sonando" de
    // "decayó a silencio".
    std::vector<std::int16_t> pcm(4000, 10000);

    // Polifonía pequeña y deliberadamente configurada (§17: ya no es
    // una constante hardcodeada) para poder forzar, con pocos samples
    // cargados, que TODAS las voces del pool terminen en la MISMA
    // nota — más capas que el viejo límite fijo de 8.
    constexpr std::size_t kPolyphony = 16;
    auto engine = createSamplerEngine(kSampleRate, kPolyphony);

    auto mapping = createKeyVelocityMapping();
    // 16 sampleIds distintos, todos mapeados a la MISMA nota/rango de
    // velocity -> un solo noteOn dispara las 16 capas a la vez.
    std::vector<MappingZone> zones;
    for (int sampleId = 1; sampleId <= static_cast<int>(kPolyphony); ++sampleId) {
        zones.push_back(MappingZone{60, 60, 0, 127, static_cast<std::uint64_t>(sampleId)});
    }
    mapping->setZones(zones);
    engine->setMapping(mapping.get());

    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    source.loopEnabled = true;
    source.loopStartFrame = 0;
    source.loopEndFrame = static_cast<std::uint32_t>(pcm.size());
    for (int sampleId = 1; sampleId <= static_cast<int>(kPolyphony); ++sampleId) {
        engine->loadSample(static_cast<std::uint64_t>(sampleId), source);
    }

    engine->noteOn(60, 100);  // dispara las 16 capas simultáneamente, más que el viejo límite de 8

    std::vector<float> warmup(4096, 0.0f);
    engine->renderBlock(warmup.data(), warmup.size());
    float sustainLevel = std::abs(warmup.back());
    assert(sustainLevel > 0.1f);  // las 16 capas deben estar sonando (Sustain)

    engine->noteOff(60);  // debe apagar las 16 capas, no solo las primeras 8

    std::vector<float> releaseBuffer(10000, 0.0f);
    engine->renderBlock(releaseBuffer.data(), releaseBuffer.size());
    float tailLevel = std::abs(releaseBuffer.back());
    assert(tailLevel < 0.01f);  // silencio real: ninguna capa quedó huérfana sonando

    std::printf(
        "[tests/sampler] %zu capas en una nota (> viejo límite fijo de 8), "
        "noteOff las apaga a TODAS, sin huérfanas OK\n",
        kPolyphony);
}

}  // namespace

int main() {
    testManyLayersOnOneNoteAllReleaseOnNoteOff();
    std::printf("[tests/sampler] layer_orphan_test: TODOS los tests pasaron\n");
    return 0;
}
