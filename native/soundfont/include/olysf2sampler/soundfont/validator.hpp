#pragma once
// native/soundfont/validation — comprobación de integridad
// semántica de un SoundFontModel (rangos válidos, referencias
// cruzadas correctas entre presets/instrumentos/samples, límites de
// la especificación SF2). No modifica el modelo.

#include <memory>
#include <string>
#include <vector>

#include "olysf2sampler/soundfont/model.hpp"

namespace olysf2sampler::soundfont {

struct ValidationIssue {
    std::string message;
    bool isFatal{false};
};

struct ValidationReport {
    bool isValid{true};
    std::vector<ValidationIssue> issues;
};

class Sf2Validator {
public:
    virtual ~Sf2Validator() = default;

    /// NO noexcept: ruta offline. Ver fix Fase A.1 §9 en parser.hpp.
    virtual ValidationReport validate(const SoundFontModel& model) const = 0;
};

/// Construye la implementación real de Sf2Validator (rangos + referencias
/// cruzadas Preset->Instrument->Sample).
std::unique_ptr<Sf2Validator> createDefaultSf2Validator();

}  // namespace olysf2sampler::soundfont
