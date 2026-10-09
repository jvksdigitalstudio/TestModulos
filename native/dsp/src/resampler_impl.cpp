#include "olysf2sampler/dsp/resampler.hpp"

#include "olysf2sampler/dsp/interpolation.hpp"

namespace olysf2sampler::dsp {

namespace {

class BufferResamplerImpl final : public Resampler {
public:
    explicit BufferResamplerImpl(bool highQuality)
        : method_(highQuality ? InterpolationMethod::Cubic : InterpolationMethod::Linear) {}

    void setRatio(double outputToInputRatio) noexcept override {
        // ratio = frames de salida por frame de entrada. El paso de
        // lectura en el buffer de entrada es su inverso.
        readStep_ = outputToInputRatio > 0.0 ? 1.0 / outputToInputRatio : 1.0;
    }

    std::size_t process(const float* input, std::size_t inputFrames, float* output,
                        std::size_t outputCapacity) noexcept override {
        if (input == nullptr || output == nullptr || inputFrames == 0) {
            return 0;
        }
        double position = 0.0;
        std::size_t written = 0;
        while (written < outputCapacity && position < static_cast<double>(inputFrames - 1)) {
            std::size_t idx = static_cast<std::size_t>(position);
            float frac = static_cast<float>(position - static_cast<double>(idx));

            float y0 = idx > 0 ? input[idx - 1] : input[idx];
            float y1 = input[idx];
            float y2 = idx + 1 < inputFrames ? input[idx + 1] : input[idx];
            float y3 = idx + 2 < inputFrames ? input[idx + 2] : y2;

            output[written] = interpolateSample(method_, y0, y1, y2, y3, frac);
            ++written;
            position += readStep_;
        }
        return written;
    }

    void reset() noexcept override {
        // Sin estado persistente entre llamadas por diseño (ver nota
        // en resampler.hpp) — no hay nada que reiniciar salvo el ratio,
        // que se conserva deliberadamente (configuración, no estado
        // de reproducción).
    }

private:
    InterpolationMethod method_;
    double readStep_{1.0};
};

}  // namespace

std::unique_ptr<Resampler> createResampler(bool highQuality) {
    return std::make_unique<BufferResamplerImpl>(highQuality);
}

}  // namespace olysf2sampler::dsp
