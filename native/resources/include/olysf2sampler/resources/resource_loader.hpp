#pragma once
// native/resources — carga de recursos del MOTOR (no de UI:
// esto no es Android resources/drawables). Responsabilidad única:
// abstraer de dónde vienen los bytes (asset, filesystem, memoria) para
// que soundfont/samples no necesiten saberlo.

#include <cstdint>
#include <string>
#include <vector>

#include "olysf2sampler/core/result.hpp"

namespace olysf2sampler::resources {

class ResourceLoader {
public:
    virtual ~ResourceLoader() = default;

    virtual olysf2sampler::core::Result<std::vector<std::uint8_t>> load(
        const std::string& resourceId) noexcept = 0;
};

}  // namespace olysf2sampler::resources
