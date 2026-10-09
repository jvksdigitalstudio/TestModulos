#pragma once
// Versión del núcleo OlySf2 Sampler. Único lugar donde se declara; cualquier
// módulo o la capa JNI que necesite reportar versión debe incluir este
// header en vez de hardcodear el string en otro lugar.

namespace olysf2sampler::core {

struct EngineVersion {
    static constexpr int major = 0;
    static constexpr int minor = 1;
    static constexpr int patch = 0;
    static constexpr const char* label = "scaffold";
};

const char* versionString();

}  // namespace olysf2sampler::core
