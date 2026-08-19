#include "olysf2sampler/core/version.hpp"

#include <string>

namespace olysf2sampler::core {

const char* versionString() {
    static const std::string cached = std::to_string(EngineVersion::major) + "." +
                                       std::to_string(EngineVersion::minor) + "." +
                                       std::to_string(EngineVersion::patch) + "-" +
                                       EngineVersion::label;
    return cached.c_str();
}

}  // namespace olysf2sampler::core
