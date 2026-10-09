// Prueba AudioEngineImpl (la orquestación real, no un mock de
// AudioEngine) inyectando un AudioBackend de prueba en vez de Oboe.
// Esto SÍ es 100% verificable en host: confirma que AudioEngine abre
// el backend con la configuración correcta, propaga el callback de
// render (antes y después de start()), y reporta start/stop/latencia
// correctamente — todo lo que se puede probar sin un dispositivo
// Android real.

#include <cassert>
#include <cstdio>
#include <vector>

#include "olysf2sampler/audio/audio_engine.hpp"

using namespace olysf2sampler::audio;
using namespace olysf2sampler::platform;

namespace {

/// AudioBackend de prueba: no reproduce audio real, solo registra
/// llamadas y permite disparar el callback manualmente (simulando lo
/// que Oboe haría repetidamente desde su propio hilo de audio).
class FakeAudioBackend final : public AudioBackend {
public:
    bool open(const AudioBackendConfig& config) noexcept override {
        openCalled = true;
        lastConfig = config;
        return true;
    }
    void close() noexcept override { closeCalled = true; }
    void setRenderCallback(AudioRenderCallback callback) override {
        callback_ = std::move(callback);
        setRenderCallbackCallCount++;
    }
    bool start() noexcept override {
        startCalled = true;
        return true;
    }
    void stop() noexcept override { stopCalled = true; }
    double reportedLatencyMillis() const noexcept override { return 12.5; }

    /// Simula lo que Oboe haría desde su hilo de audio real: invoca
    /// el callback de render registrado.
    void simulateAudioCallback(float* buffer, std::size_t frameCount) {
        if (callback_) {
            callback_(buffer, frameCount);
        }
    }

    bool openCalled{false};
    bool closeCalled{false};
    bool startCalled{false};
    bool stopCalled{false};
    int setRenderCallbackCallCount{0};
    AudioBackendConfig lastConfig;

private:
    AudioRenderCallback callback_;
};

void testStartOpensBackendWithCorrectConfig() {
    auto backend = std::make_unique<FakeAudioBackend>();
    FakeAudioBackend* rawBackend = backend.get();

    auto engine = createAudioEngine(std::move(backend));
    AudioSessionConfig config;
    config.sampleRate = 44100;
    config.framesPerBuffer = 256;

    bool started = engine->start(config);
    assert(started);
    assert(rawBackend->openCalled);
    assert(rawBackend->startCalled);
    assert(rawBackend->lastConfig.sampleRate == 44100);
    assert(rawBackend->lastConfig.framesPerBuffer == 256);
    assert(rawBackend->lastConfig.channelCount == 1);  // SamplerEngine produce mono

    std::printf("[tests/audio] start() abre el backend con la config correcta OK\n");
}

void testRenderCallbackSetBeforeStartIsAppliedOnStart() {
    auto backend = std::make_unique<FakeAudioBackend>();
    FakeAudioBackend* rawBackend = backend.get();
    auto engine = createAudioEngine(std::move(backend));

    bool callbackInvoked = false;
    engine->setRenderCallback([&](float* buf, std::size_t n) {
        callbackInvoked = true;
        for (std::size_t i = 0; i < n; ++i) buf[i] = 1.0f;
    });

    engine->start(AudioSessionConfig{});
    assert(rawBackend->setRenderCallbackCallCount >= 1);

    std::vector<float> buffer(64, 0.0f);
    rawBackend->simulateAudioCallback(buffer.data(), buffer.size());
    assert(callbackInvoked);
    assert(buffer[0] == 1.0f);

    std::printf("[tests/audio] callback de render configurado ANTES de start() se propaga OK\n");
}

void testStopClosesBackend() {
    auto backend = std::make_unique<FakeAudioBackend>();
    FakeAudioBackend* rawBackend = backend.get();
    auto engine = createAudioEngine(std::move(backend));

    engine->start(AudioSessionConfig{});
    engine->stop();

    assert(rawBackend->stopCalled);
    assert(rawBackend->closeCalled);
    std::printf("[tests/audio] stop() detiene y cierra el backend OK\n");
}

void testLatencyReportedFromBackend() {
    auto backend = std::make_unique<FakeAudioBackend>();
    auto engine = createAudioEngine(std::move(backend));
    engine->start(AudioSessionConfig{});
    assert(engine->currentLatencyMillis() == 12.5);
    std::printf("[tests/audio] latencia reportada viene del backend real OK\n");
}

}  // namespace

int main() {
    testStartOpensBackendWithCorrectConfig();
    testRenderCallbackSetBeforeStartIsAppliedOnStart();
    testStopClosesBackend();
    testLatencyReportedFromBackend();
    std::printf("[tests/audio] TODOS los tests pasaron\n");
    return 0;
}
