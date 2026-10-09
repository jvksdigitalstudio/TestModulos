#include "olysf2sampler/audio/audio_engine.hpp"

namespace olysf2sampler::audio {

namespace {

class AudioEngineImpl final : public AudioEngine {
public:
    explicit AudioEngineImpl(std::unique_ptr<platform::AudioBackend> backend)
        : backend_(std::move(backend)) {}

    bool start(const AudioSessionConfig& config) noexcept override {
        if (!backend_) {
            return false;
        }
        platform::AudioBackendConfig backendConfig;
        backendConfig.sampleRate = config.sampleRate;
        backendConfig.framesPerBuffer = config.framesPerBuffer;
        backendConfig.channelCount = 1;  // SamplerEngine::renderBlock produce mono (ver nota)

        if (!backend_->open(backendConfig)) {
            return false;
        }
        if (pendingCallback_) {
            backend_->setRenderCallback(pendingCallback_);
        }
        bool started = backend_->start();
        running_ = started;
        return started;
    }

    void stop() noexcept override {
        if (!backend_) {
            return;
        }
        backend_->stop();
        backend_->close();
        running_ = false;
    }

    void setRenderCallback(platform::AudioRenderCallback callback) override {
        pendingCallback_ = std::move(callback);
        if (backend_ && running_) {
            backend_->setRenderCallback(pendingCallback_);
        }
    }

    double currentLatencyMillis() const noexcept override {
        return backend_ ? backend_->reportedLatencyMillis() : 0.0;
    }

private:
    std::unique_ptr<platform::AudioBackend> backend_;
    platform::AudioRenderCallback pendingCallback_;
    bool running_{false};
};

}  // namespace

std::unique_ptr<AudioEngine> createAudioEngine(std::unique_ptr<platform::AudioBackend> backend) {
    return std::make_unique<AudioEngineImpl>(std::move(backend));
}

}  // namespace olysf2sampler::audio
