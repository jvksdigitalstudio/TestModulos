#pragma once
// native/soundfont/writer — serializa SoundFontModel a bytes SF2.
// Responsabilidad única: escribir. No valida semánticamente antes de
// escribir (el llamador debe invocar Sf2Validator explícitamente).

#include <cstdint>
#include <memory>
#include <vector>

#include "olysf2sampler/core/result.hpp"
#include "olysf2sampler/soundfont/model.hpp"

namespace olysf2sampler::soundfont {

class Sf2Writer {
public:
    virtual ~Sf2Writer() = default;

    /// NO noexcept: ruta offline (no realtime). Ver fix Fase A.1 §9
    /// en parser.hpp — misma razón exacta aquí.
    virtual olysf2sampler::core::Result<std::vector<std::uint8_t>> write(
        const SoundFontModel& model) = 0;
};

/// Construye la implementación real (RIFF/INFO/sdta/pdta) de Sf2Writer.
std::unique_ptr<Sf2Writer> createDefaultSf2Writer();

}  // namespace olysf2sampler::soundfont
