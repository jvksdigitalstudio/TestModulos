// native/platform/src/oboe_audio_backend.cpp
//
// ADVERTENCIA DE VERIFICACIÓN — léase antes de tocar este archivo:
//
// Este es el ÚNICO archivo de todo el proyecto que no se ha podido
// compilar ni ejecutar en el entorno donde se escribió (sin acceso de
// red para descargar Oboe). Todo lo demás en este repositorio fue
// compilado, ejecutado, y en la mayoría de los casos verificado bajo
// AddressSanitizer/UndefinedBehaviorSanitizer/ThreadSanitizer antes de
// entregarse. Este archivo se escribió con el mismo cuidado y
// siguiendo la API pública documentada de Oboe (github.com/google/oboe),
// pero su primera compilación real ocurre en CI (GitHub Actions SÍ
// tiene acceso de red — ver native/platform/CMakeLists.txt, que
// descarga Oboe vía FetchContent solo cuando ANDROID=ON).
//
// Si la build de CI falla aquí, es el punto exacto por el que
// empezar a mirar el log — ver docs/architecture/AUDIO_ENGINE.md.

#ifdef __ANDROID__

#include "olysf2sampler/platform/audio_backend.hpp"

#include <oboe/Oboe.h>

namespace olysf2sampler::platform {

namespace {

class RenderCallbackAdapter final : public oboe::AudioStreamCallback {
public:
    void setRenderCallback(AudioRenderCallback callback) { renderCallback_ = std::move(callback); }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* /*stream*/, void* audioData,
                                          int32_t numFrames) override {
        float* buffer = static_cast<float*>(audioData);
        if (renderCallback_) {
            // El callback de render (típicamente sampler::SamplerEngine::
            // renderBlock) espera un buffer ya puesto a cero si quiere
            // acumular voces; se limpia aquí antes de invocarlo.
            for (int32_t i = 0; i < numFrames; ++i) {
                buffer[i] = 0.0f;
            }
            renderCallback_(buffer, static_cast<std::size_t>(numFrames));
        } else {
            for (int32_t i = 0; i < numFrames; ++i) {
                buffer[i] = 0.0f;
            }
        }
        return oboe::DataCallbackResult::Continue;
    }

private:
    AudioRenderCallback renderCallback_;
};

class OboeAudioBackend final : public AudioBackend {
public:
    bool open(const AudioBackendConfig& config) noexcept override {
        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(config.channelCount)
            ->setSampleRate(config.sampleRate)
            ->setCallback(&callbackAdapter_);

        if (config.framesPerBuffer > 0) {
            builder.setFramesPerCallback(config.framesPerBuffer);
        }

        oboe::Result result = builder.openStream(stream_);
        return result == oboe::Result::OK;
    }

    void close() noexcept override {
        if (stream_) {
            stream_->close();
            stream_.reset();
        }
    }

    void setRenderCallback(AudioRenderCallback callback) override {
        callbackAdapter_.setRenderCallback(std::move(callback));
    }

    bool start() noexcept override {
        if (!stream_) {
            return false;
        }
        return stream_->requestStart() == oboe::Result::OK;
    }

    void stop() noexcept override {
        if (stream_) {
            stream_->requestStop();
        }
    }

    double reportedLatencyMillis() const noexcept override {
        if (!stream_) {
            return 0.0;
        }
        oboe::ResultWithValue<double> result = stream_->calculateLatencyMillis();
        return result ? result.value() : 0.0;
    }

private:
    RenderCallbackAdapter callbackAdapter_;
    std::shared_ptr<oboe::AudioStream> stream_;
};

}  // namespace

std::unique_ptr<AudioBackend> createOboeAudioBackend() {
    return std::make_unique<OboeAudioBackend>();
}

}  // namespace olysf2sampler::platform

#endif  // __ANDROID__
