#pragma once
// native/sampler/modulation — conexión entre fuentes de
// modulación (LFO, envolvente, velocity, aftertouch) y destinos
// (pitch, filtro, ganancia). Usa olysf2sampler::dsp como fuente de valores,
// pero DSP no conoce esto (dependencia unidireccional).

#include <cstdint>

namespace olysf2sampler::sampler {

enum class ModulationSource {
    Lfo1,
    EnvelopeFilter,
    EnvelopeAmplitude,
    Velocity,
    Aftertouch,
};

enum class ModulationDestination {
    Pitch,
    FilterCutoff,
    Amplitude,
};

struct ModulationRoute {
    ModulationSource source;
    ModulationDestination destination;
    float amount{0.0f};
};

class Modulator {
public:
    virtual ~Modulator() = default;

    virtual void addRoute(const ModulationRoute& route) = 0;
    virtual void clearRoutes() noexcept = 0;
};

}  // namespace olysf2sampler::sampler
