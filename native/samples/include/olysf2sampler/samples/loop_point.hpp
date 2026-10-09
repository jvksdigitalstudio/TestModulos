#pragma once
// native/samples/loop — definición y detección de loop points.
// Responsabilidad única: modelar y (en fases posteriores) detectar
// puntos de loop; no reproduce audio (eso es sampler::Voice).

#include <cstdint>

namespace olysf2sampler::samples {

enum class LoopMode {
    NoLoop,
    Forward,
    PingPong,
};

struct LoopPoint {
    LoopMode mode{LoopMode::NoLoop};
    std::uint32_t startFrame{0};
    std::uint32_t endFrame{0};
};

}  // namespace olysf2sampler::samples
