// tests/sampler/volume_ramp_test.cpp
//
// Fase A.1 §16 (fix real): antes, SamplerEngine::setMasterVolume
// aplicaba el nuevo volumen como un escalón completo al inicio del
// siguiente bloque ("zipper noise"). Ahora rampea linealmente muestra
// a muestra a lo largo de rampMs, incluso cruzando varios
// renderBlock() si rampMs es más largo que un bloque. Este test
// confirma ambos comportamientos con audio real, no solo que compila.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;

// engine y mapping deben vivir juntos en el scope del llamador:
// SamplerEngine::setMapping() toma un puntero NO propietario (documentado
// en sampler_engine.hpp) — devolver solo el engine desde una función
// helper dejaría `mapping` destruido al retornar, con el engine
// apuntando a memoria liberada.
struct EngineWithMapping {
    std::unique_ptr<KeyVelocityMapping> mapping;
    std::unique_ptr<SamplerEngine> engine;
};

EngineWithMapping makeEngineWithConstantTone() {
    EngineWithMapping ewm;
    ewm.engine = createSamplerEngine(kSampleRate);
    ewm.mapping = createKeyVelocityMapping();
    ewm.mapping->setZones({MappingZone{0, 127, 0, 127, /*sampleId=*/1}});
    ewm.engine->setMapping(ewm.mapping.get());
    return ewm;
}

// pcm vive en el scope del llamador (SampleSource no es propietario).
void loadConstantTone(SamplerEngine& engine, std::vector<std::int16_t>& pcm) {
    pcm.assign(8000, 16000);  // valor constante alto: fácil medir escalones
    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    source.loopEnabled = true;
    source.loopStartFrame = 0;
    source.loopEndFrame = static_cast<std::uint32_t>(pcm.size());
    engine.loadSample(1, source);
}

double maxSampleToSampleJump(const std::vector<float>& buffer) {
    double maxJump = 0.0;
    for (std::size_t i = 1; i < buffer.size(); ++i) {
        maxJump = std::max(maxJump, static_cast<double>(std::abs(buffer[i] - buffer[i - 1])));
    }
    return maxJump;
}

void testRampMsZeroKeepsOldBehaviorEssentiallyInstant() {
    std::vector<std::int16_t> pcm;
    EngineWithMapping ewm = makeEngineWithConstantTone();
    SamplerEngine& engine = *ewm.engine;
    loadConstantTone(engine, pcm);
    engine.noteOn(60, 100);

    std::vector<float> warmup(2048, 0.0f);
    engine.renderBlock(warmup.data(), warmup.size());  // llega a sustain con volumen 1.0

    engine.setMasterVolume(0.1f);  // rampMs default = 0.0f
    std::vector<float> afterStep(2048, 0.0f);
    engine.renderBlock(afterStep.data(), afterStep.size());

    // Con rampMs=0, casi todo el bloque debe estar ya en el nuevo
    // nivel (solo 1 frame de transición) — el pico del bloque debe
    // reflejar ~0.1x, no seguir en ~1.0x.
    float peak = 0.0f;
    for (float s : afterStep) peak = std::max(peak, std::abs(s));
    assert(peak < 0.3f);  // mucho más cerca de 0.1x que de 1.0x

    std::printf("[tests/sampler] setMasterVolume sin rampMs sigue siendo esencialmente "
                "inmediato (pico %.3f) OK\n",
                peak);
}

void testRampMsSpanningMultipleBlocksIsSmoothAcrossBlocks() {
    std::vector<std::int16_t> pcm;
    EngineWithMapping ewm = makeEngineWithConstantTone();
    SamplerEngine& engine = *ewm.engine;
    loadConstantTone(engine, pcm);
    engine.noteOn(60, 100);

    constexpr std::size_t kBlockSize = 256;  // bloque típico, pequeño
    std::vector<float> warmup(kBlockSize, 0.0f);
    engine.renderBlock(warmup.data(), warmup.size());  // sustain a volumen 1.0

    // rampMs bien más largo que un bloque: a 48kHz, 256 frames = ~5.3ms.
    // Pedimos una rampa de 50ms (~2400 frames, ~9-10 bloques de 256).
    constexpr float kRampMs = 50.0f;
    engine.setMasterVolume(0.0f, kRampMs);  // rampa completa a silencio

    double maxJumpAcrossAllBlocks = 0.0;
    bool sawIntermediateLevel = false;
    for (int block = 0; block < 15; ++block) {
        std::vector<float> buffer(kBlockSize, 0.0f);
        engine.renderBlock(buffer.data(), buffer.size());
        maxJumpAcrossAllBlocks = std::max(maxJumpAcrossAllBlocks, maxSampleToSampleJump(buffer));
        float peak = 0.0f;
        for (float s : buffer) peak = std::max(peak, std::abs(s));
        // Un nivel "intermedio" real (ni el viejo 1.0x completo, ni ya
        // silencio) confirma que la rampa sigue en curso a través de
        // bloques, no que saltó todo en el primer bloque.
        if (peak > 0.05f && peak < 0.7f) {
            sawIntermediateLevel = true;
        }
    }

    // Un escalón de bloque completo (comportamiento viejo) saltaría de
    // ~0.48 (16000/32768) a 0.0 de una sola vez: salto de muestra a
    // muestra ~0.48. Una rampa real de 50ms repartida en miles de
    // muestras debe tener saltos muchísimo más chicos.
    assert(maxJumpAcrossAllBlocks < 0.02);
    assert(sawIntermediateLevel);  // de verdad pasó por niveles intermedios, no un salto

    std::printf(
        "[tests/sampler] setMasterVolume(0.0, rampMs=50) rampea de verdad a través de "
        "múltiples renderBlock() (salto máx muestra-a-muestra %.5f, sin escalón de %.3f) OK\n",
        maxJumpAcrossAllBlocks, 16000.0 / 32768.0);
}

}  // namespace

int main() {
    testRampMsZeroKeepsOldBehaviorEssentiallyInstant();
    testRampMsSpanningMultipleBlocksIsSmoothAcrossBlocks();
    std::printf("[tests/sampler] volume_ramp_test: TODOS los tests pasaron\n");
    return 0;
}
