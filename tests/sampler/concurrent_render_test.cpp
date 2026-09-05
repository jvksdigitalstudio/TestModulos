// Prueba la propiedad que realmente importa: noteOn/noteOff llamados
// desde un hilo de CONTROL mientras renderBlock corre repetidamente en
// otro hilo simulando el callback de audio real — al mismo tiempo, de
// verdad, no en secuencia. Se compila y corre bajo ThreadSanitizer en
// CI para detectar cualquier data race real entre ambos hilos.

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

using namespace olysf2sampler::sampler;

namespace {

constexpr float kSampleRate = 48000.0f;
constexpr int kBlockFrames = 256;
constexpr int kAudioBlocksToRender = 2000;  // ~10.6s de audio simulado
constexpr int kNoteEventsToSend = 5000;

std::vector<std::int16_t> makeTestWaveform(std::size_t frameCount) {
    std::vector<std::int16_t> data(frameCount, 1000);
    return data;
}

void testConcurrentNoteEventsWhileRendering() {
    std::vector<std::int16_t> pcm = makeTestWaveform(4000);

    auto engine = createSamplerEngine(kSampleRate);
    auto mapping = createKeyVelocityMapping();
    std::vector<MappingZone> zones;
    for (int note = 0; note < 128; ++note) {
        zones.push_back(MappingZone{note, note, 0, 127, 1});
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
    engine->loadSample(1, source);

    std::atomic<bool> controlThreadDone{false};

    // "Audio thread" simulado: renderiza bloques continuamente, tal
    // como haría el callback real de Oboe/AAudio en Fase D.
    std::thread audioThread([&]() {
        std::vector<float> buffer(kBlockFrames, 0.0f);
        for (int block = 0; block < kAudioBlocksToRender; ++block) {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            engine->renderBlock(buffer.data(), buffer.size());
        }
    });

    // "Control thread": dispara/suelta notas mientras el audio thread
    // sigue renderizando en paralelo.
    std::thread controlThread([&]() {
        for (int i = 0; i < kNoteEventsToSend; ++i) {
            int note = i % 128;
            engine->noteOn(note, 100);
            if (i % 3 == 0) {
                engine->noteOff(note);
            }
        }
        controlThreadDone.store(true, std::memory_order_release);
    });

    audioThread.join();
    controlThread.join();

    // La única aserción real es que esto termina sin que
    // ThreadSanitizer reporte una data race y sin crashear — la
    // ejecución completa hasta aquí ya es la prueba.
    std::printf(
        "[tests/sampler] %d eventos de nota concurrentes con %d bloques de audio, sin data "
        "race OK\n",
        kNoteEventsToSend, kAudioBlocksToRender);
}

}  // namespace

int main() {
    testConcurrentNoteEventsWhileRendering();
    return 0;
}
