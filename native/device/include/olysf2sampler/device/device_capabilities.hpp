#pragma once
// native/device — capacidades del dispositivo relevantes para
// audio (latencia reportada, sample rate nativo, núcleos disponibles).
// Responsabilidad única: reportar, no decidir política (la política de
// qué hacer con baja/alta latencia vive en olysf2sampler::audio).

#include <cstdint>

namespace olysf2sampler::device {

struct DeviceAudioCapabilities {
    int nativeSampleRate{48000};
    int nativeFramesPerBurst{192};
    bool supportsLowLatency{false};
    std::uint32_t availableCores{1};
};

class DeviceCapabilitiesProvider {
public:
    virtual ~DeviceCapabilitiesProvider() = default;
    virtual DeviceAudioCapabilities query() const noexcept = 0;
};

}  // namespace olysf2sampler::device
