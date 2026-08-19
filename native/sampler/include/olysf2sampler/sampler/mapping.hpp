#pragma once
// native/sampler/mapping — resolución de key/velocity mapping a
// nivel de motor. Responsabilidad única: dado (nota, velocity), decidir
// qué sample(s)/zona(s) deben sonar. La UI de edición del mapeo
// (pintar rangos en pantalla) NO vive en este módulo — pertenecerá al
// futuro anfitrión (Olyze Music Studio) cuando integre OlySf2 Sampler
// a través de EliNer; este módulo solo resuelve, no dibuja.

#include <cstdint>
#include <memory>
#include <vector>

namespace olysf2sampler::sampler {

struct MappingZone {
    int lowNote{0};
    int highNote{127};
    int lowVelocity{0};
    int highVelocity{127};
    std::uint64_t sampleId{0};
};

class KeyVelocityMapping {
public:
    virtual ~KeyVelocityMapping() = default;

    virtual void setZones(std::vector<MappingZone> zones) = 0;

    /// Devuelve los ids de sample que deben dispararse para (note, velocity).
    /// Puede haber más de uno (layering).
    virtual std::vector<std::uint64_t> resolve(int midiNote, int velocity) const noexcept = 0;
};

/// Construye la implementación real: recorrido lineal de las zonas
/// registradas (suficiente para el número de zonas típico de un
/// preset SF2 — decenas, no miles; si el perfilado futuro muestra que
/// esto pesa, se optimiza con una estructura indexada, no antes).
std::unique_ptr<KeyVelocityMapping> createKeyVelocityMapping();

}  // namespace olysf2sampler::sampler
