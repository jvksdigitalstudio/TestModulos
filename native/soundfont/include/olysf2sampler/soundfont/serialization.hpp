#pragma once
// native/soundfont/serialization — punto de fachada que combina
// parser/writer/validator para operaciones de alto nivel expuestas
// hacia la frontera JNI (p.ej. "importar .sf2", "exportar .sf2").
// No implementa parsing/writing por sí mismo: delega.

#include <cstdint>
#include <vector>

#include "olysf2sampler/core/result.hpp"
#include "olysf2sampler/soundfont/model.hpp"
#include "olysf2sampler/soundfont/parser.hpp"
#include "olysf2sampler/soundfont/writer.hpp"

namespace olysf2sampler::soundfont {

class Sf2SerializationService {
public:
    Sf2SerializationService(Sf2Parser& parser, Sf2Writer& writer)
        : parser_(parser), writer_(writer) {}

    olysf2sampler::core::Result<SoundFontModel> importFromBytes(const ByteSpan& data) noexcept {
        return parser_.parse(data);
    }

    olysf2sampler::core::Result<std::vector<std::uint8_t>> exportToBytes(
        const SoundFontModel& model) noexcept {
        return writer_.write(model);
    }

private:
    Sf2Parser& parser_;
    Sf2Writer& writer_;
};

}  // namespace olysf2sampler::soundfont
