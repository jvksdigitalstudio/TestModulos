#pragma once
// native/samples/io — lectura/escritura de audio de samples
// (WAV y formatos soportados). Responsabilidad única: I/O de audio
// crudo. No procesa (normalización/trim) ni conoce loops.

#include <cstdint>
#include <vector>

#include "olysf2sampler/core/result.hpp"

namespace olysf2sampler::samples {

struct PcmBuffer {
    std::vector<float> interleavedSamples;
    std::uint32_t sampleRate{44100};
    std::uint16_t channelCount{1};
};

class SampleReader {
public:
    virtual ~SampleReader() = default;
    virtual olysf2sampler::core::Result<PcmBuffer> read(const std::uint8_t* data,
                                                   std::size_t size) noexcept = 0;
};

class SampleWriter {
public:
    virtual ~SampleWriter() = default;
    virtual olysf2sampler::core::Result<std::vector<std::uint8_t>> write(
        const PcmBuffer& buffer) noexcept = 0;
};

}  // namespace olysf2sampler::samples
