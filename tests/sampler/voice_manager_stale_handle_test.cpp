// tests/sampler/voice_manager_stale_handle_test.cpp
//
// Regresión específica para el bug P0 de Fase A.1 §12: un VoiceId
// obtenido para la nota A no debe poder afectar a la voz que, tras un
// voice-steal, pasó a pertenecer a la nota B. Antes del fix, VoiceId
// era simplemente "slot índice + 1", así que un noteOff tardío sobre
// la nota A robada apagaba silenciosamente la nota B en curso. Con el
// fix (generación empaquetada en el handle), ese release debe ser un
// no-op seguro.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olysf2sampler/sampler/voice_manager.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;

SampleSource makeConstantLoopedSource(const std::vector<std::int16_t>& pcm) {
    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    source.loopEnabled = true;
    source.loopStartFrame = 0;
    source.loopEndFrame = static_cast<std::uint32_t>(pcm.size());
    return source;
}

void testStaleVoiceIdAfterStealingDoesNotReleaseNewOwner() {
    // Señal constante: fácil distinguir "sigue sonando en Sustain" de
    // "está decayendo en Release hacia silencio".
    std::vector<std::int16_t> pcm(4000, 10000);
    SampleSource source = makeConstantLoopedSource(pcm);

    auto manager = createVoiceManager(kSampleRate);
    manager->setMaxPolyphony(1);  // un único slot: fuerza robo en el 2do trigger

    NoteEvent eventA;
    eventA.midiNote = 60;
    eventA.velocity = 100;
    eventA.sampleId = 1;
    VoiceId idA = manager->acquireVoice(eventA, source);
    assert(idA != kInvalidVoiceId);

    // Deja que A llegue a Sustain antes de robarla.
    std::vector<float> warmup(4096, 0.0f);
    manager->renderBlock(warmup.data(), warmup.size());

    NoteEvent eventB;
    eventB.midiNote = 67;
    eventB.velocity = 100;
    eventB.sampleId = 1;
    VoiceId idB = manager->acquireVoice(eventB, source);  // roba el único slot (era de A)
    assert(idB != kInvalidVoiceId);
    assert(idB != idA);  // mismo slot, pero handle distinto (nueva generación)

    // Deja que B llegue también a Sustain.
    manager->renderBlock(warmup.data(), warmup.size());

    // noteOff tardío sobre A: el handle es stale porque el slot que
    // ocupaba ahora pertenece a B. No debe afectar a B.
    manager->releaseVoice(idA);

    std::vector<float> afterStaleRelease(4096, 0.0f);
    manager->renderBlock(afterStaleRelease.data(), afterStaleRelease.size());
    float levelAfterStaleRelease = std::abs(afterStaleRelease.back());
    assert(levelAfterStaleRelease > 0.1f);  // B debe seguir en Sustain, no decayendo

    // Control positivo: releaseVoice(idB), el handle correcto, sí debe
    // iniciar el release real y decaer a silencio.
    manager->releaseVoice(idB);
    std::vector<float> afterRealRelease(10000, 0.0f);
    manager->renderBlock(afterRealRelease.data(), afterRealRelease.size());
    float levelAfterRealRelease = std::abs(afterRealRelease.back());
    assert(levelAfterRealRelease < 0.01f);

    std::printf(
        "[tests/sampler] stale VoiceId tras voice-steal no afecta a la voz nueva; "
        "release real sigue funcionando OK\n");
}

}  // namespace

int main() {
    testStaleVoiceIdAfterStealingDoesNotReleaseNewOwner();
    std::printf("[tests/sampler] voice_manager_stale_handle_test: TODOS los tests pasaron\n");
    return 0;
}
