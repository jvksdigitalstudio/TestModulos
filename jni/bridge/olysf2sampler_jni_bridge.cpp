#include "olysf2sampler_jni_bridge.hpp"

#include <cmath>
#include <memory>
#include <vector>

#include "olysf2sampler/audio/audio_engine.hpp"
#include "olysf2sampler/platform/audio_backend.hpp"
#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

// Este archivo es la UNICA frontera JNI del motor: se limita a
// construir/destruir el nucleo y convertir tipos primitivos. La
// logica real (mezcla de voces, DSP, RIFF) vive enteramente en
// native/ y se invoca desde aqui, nunca se reimplementa aqui (Fase 1
// Sec.9 / Fase A Sec.5, siguen vigentes).
//
// Estado global de modulo: se asume una unica instancia del motor por
// proceso (consistente con la suposicion ya documentada de "un solo
// productor" en la cola SPSC de SamplerEngine -- ver
// docs/architecture/THREADING.md). Si en el futuro se necesitan
// multiples instancias independientes, esto se reemplaza por un
// handle (jlong) devuelto por nativeInitialize y pasado de vuelta en
// cada llamada -- no se construye esa generalidad ahora sin un caso
// de uso real (Fase 1 Sec.32, anti-overengineering).

namespace {

constexpr float kToneFrequencyHz = 440.0f;    // A4, tono de prueba reconocible
constexpr float kToneDurationSeconds = 2.0f;  // con loop, no hace falta mas
constexpr std::uint64_t kTestToneSampleId = 1;
constexpr std::uint8_t kTestToneRootNote = 69;  // A4 = MIDI 69

std::vector<std::int16_t> g_testTonePcm;
std::unique_ptr<olysf2sampler::sampler::KeyVelocityMapping> g_mapping;
std::unique_ptr<olysf2sampler::sampler::SamplerEngine> g_samplerEngine;
std::unique_ptr<olysf2sampler::audio::AudioEngine> g_audioEngine;

std::vector<std::int16_t> generateTestTone(float sampleRateHz) {
    std::size_t frameCount = static_cast<std::size_t>(sampleRateHz * kToneDurationSeconds);
    std::vector<std::int16_t> pcm(frameCount);
    for (std::size_t i = 0; i < frameCount; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(sampleRateHz);
        double value = std::sin(2.0 * 3.14159265358979323846 * kToneFrequencyHz * t);
        pcm[i] = static_cast<std::int16_t>(value * 8000.0);  // amplitud moderada
    }
    return pcm;
}

// Fix (auditoría de lifetime, JNI/native boundary): detiene y libera
// cualquier instancia previa ANTES de construir una nueva. Es
// exactamente lo que hacía nativeShutdown, pero factorizado aquí
// porque nativeInitialize también lo necesita.
//
// Motivo: antes de este fix, nativeInitialize reasignaba
// g_samplerEngine/g_audioEngine directamente. Si se llama dos veces
// sin un nativeShutdown() de por medio (lifecycle de Android real:
// una reconfiguración, un caller con un bug, dos hilos llamando
// initialize casi a la vez) el audio thread del backend Oboe ANTERIOR
// podía seguir ejecutando su callback (`g_samplerEngine->renderBlock`)
// en el momento exacto en que `g_samplerEngine = createSamplerEngine(...)`
// destruye el objeto viejo vía std::unique_ptr::operator= — use-after-free
// real en el audio thread, no hipotético. `stopAndReleaseCurrentEngine()`
// llama a `AudioEngine::stop()` (que detiene el backend de forma
// síncrona antes de retornar, ver native/audio/src/audio_engine_impl.cpp)
// ANTES de tocar g_samplerEngine, así que para cuando se reasigna ya
// no hay ningún callback de audio en vuelo.
void stopAndReleaseCurrentEngine() {
    if (g_audioEngine) {
        g_audioEngine->stop();
    }
    g_audioEngine.reset();
    g_samplerEngine.reset();
    g_mapping.reset();
    g_testTonePcm.clear();
}

}  // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_yeivikas_olysf2sampler_internal_NativeOlySf2SamplerEngine_nativeInitialize(
    JNIEnv* /*env*/, jobject /*thiz*/, jint preferredSampleRate) {
    // Fix: garantiza que cualquier instancia previa (y su audio thread,
    // si estaba corriendo) esté completamente detenida y destruida
    // ANTES de construir la nueva — ver comentario de
    // stopAndReleaseCurrentEngine() arriba. Sin esto, una llamada
    // doble a nativeInitialize sin nativeShutdown de por medio podía
    // destruir un SamplerEngine mientras el audio thread anterior
    // todavía lo estaba usando.
    stopAndReleaseCurrentEngine();

    float sampleRateHz =
        preferredSampleRate > 0 ? static_cast<float>(preferredSampleRate) : 48000.0f;

    g_testTonePcm = generateTestTone(sampleRateHz);

    g_mapping = olysf2sampler::sampler::createKeyVelocityMapping();
    g_mapping->setZones(
        {olysf2sampler::sampler::MappingZone{0, 127, 0, 127, kTestToneSampleId}});

    g_samplerEngine = olysf2sampler::sampler::createSamplerEngine(sampleRateHz);
    g_samplerEngine->setMapping(g_mapping.get());

    olysf2sampler::sampler::SampleSource source;
    source.pcmData = g_testTonePcm.data();
    source.frameCount = g_testTonePcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(sampleRateHz);
    source.rootNote = kTestToneRootNote;
    source.loopEnabled = true;
    source.loopStartFrame = 0;
    source.loopEndFrame = static_cast<std::uint32_t>(g_testTonePcm.size());
    g_samplerEngine->loadSample(kTestToneSampleId, source);

    auto backend = olysf2sampler::platform::createOboeAudioBackend();
    g_audioEngine = olysf2sampler::audio::createAudioEngine(std::move(backend));
    g_audioEngine->setRenderCallback([](float* outputBuffer, std::size_t frameCount) {
        g_samplerEngine->renderBlock(outputBuffer, frameCount);
    });

    olysf2sampler::audio::AudioSessionConfig config;
    config.sampleRate = static_cast<int>(sampleRateHz);
    bool started = g_audioEngine->start(config);
    return started ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_yeivikas_olysf2sampler_internal_NativeOlySf2SamplerEngine_nativeShutdown(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    stopAndReleaseCurrentEngine();
}

JNIEXPORT void JNICALL
Java_com_yeivikas_olysf2sampler_internal_NativeOlySf2SamplerEngine_nativeNoteOn(
    JNIEnv* /*env*/, jobject /*thiz*/, jint midiNote, jint velocity) {
    if (g_samplerEngine) {
        g_samplerEngine->noteOn(static_cast<int>(midiNote), static_cast<int>(velocity));
    }
}

JNIEXPORT void JNICALL
Java_com_yeivikas_olysf2sampler_internal_NativeOlySf2SamplerEngine_nativeNoteOff(
    JNIEnv* /*env*/, jobject /*thiz*/, jint midiNote) {
    if (g_samplerEngine) {
        g_samplerEngine->noteOff(static_cast<int>(midiNote));
    }
}

}  // extern "C"
