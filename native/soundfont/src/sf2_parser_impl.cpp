#include "olysf2sampler/soundfont/parser.hpp"

#include <optional>
#include <string>
#include <vector>

#include "sf2_binary_format.hpp"

namespace olysf2sampler::soundfont {

namespace {

using detail::checkedMul;
using detail::SafeByteReader;
using detail::kIbagRecordSize;
using detail::kIgenRecordSize;
using detail::kImodRecordSize;
using detail::kInstRecordSize;
using detail::kPbagRecordSize;
using detail::kPgenRecordSize;
using detail::kPhdrRecordSize;
using detail::kPmodRecordSize;
using detail::kShdrRecordSize;

using core::Error;
using core::ErrorCode;
using core::Result;

// Límite defensivo, deliberadamente generoso (ver docs/architecture/
// FILE_AUDIT.md §31): protege contra recuentos patológicos sin
// castrar SoundFonts grandes y legítimos.
constexpr std::size_t kMaxReasonableRecordCount = 5'000'000;

Result<SoundFontModel> fail(ErrorCode code, std::string message) {
    return Result<SoundFontModel>::fail(Error{code, std::move(message)});
}

struct RawChunk {
    std::string id;
    ByteSpan data;
};

std::optional<RawChunk> readChunk(SafeByteReader& reader) {
    auto id = reader.readFourCC();
    if (!id) {
        return std::nullopt;
    }
    auto size = reader.readU32LE();
    if (!size) {
        return std::nullopt;
    }
    auto payload = reader.readSpan(*size);
    if (!payload) {
        return std::nullopt;
    }
    if ((*size % 2) == 1) {
        // Byte de padding para alinear a 2 bytes; puede no existir si el
        // chunk termina justo en el final del archivo — no es fatal.
        [[maybe_unused]] bool paddingSkipped = reader.skip(1);
    }
    return RawChunk{*id, *payload};
}

std::optional<std::size_t> recordCountOf(const ByteSpan& chunkData, std::size_t recordSize) {
    if (recordSize == 0 || chunkData.size % recordSize != 0) {
        return std::nullopt;
    }
    std::size_t count = chunkData.size / recordSize;
    if (count > kMaxReasonableRecordCount) {
        return std::nullopt;
    }
    return count;
}

// ---- Registros crudos ----

struct RawBag {
    std::uint16_t genIndex{0};
    std::uint16_t modIndex{0};
};

struct RawPresetHeader {
    std::string name;
    std::uint16_t presetNumber{0};
    std::uint16_t bank{0};
    std::uint16_t bagIndex{0};
};

struct RawInstHeader {
    std::string name;
    std::uint16_t bagIndex{0};
};

struct RawGen {
    std::uint16_t oper{0};
    std::int16_t amount{0};
};

struct RawMod {
    std::uint16_t sourceOper{0};
    std::uint16_t destinationOper{0};
    std::int16_t amount{0};
    std::uint16_t amountSourceOper{0};
    std::uint16_t transformOper{0};
};

bool parseInfoChunk(const ByteSpan& listData, SoundFontMetadata& out) {
    SafeByteReader reader(listData);
    while (!reader.atEnd()) {
        auto chunk = readChunk(reader);
        if (!chunk) {
            return false;
        }
        SafeByteReader sub(chunk->data);
        if (chunk->id == "isng") {
            out.soundEngine = sub.readFixedString(chunk->data.size).value_or("");
        } else if (chunk->id == "INAM") {
            out.bankName = sub.readFixedString(chunk->data.size).value_or("");
        } else if (chunk->id == "IPRD") {
            out.productName = sub.readFixedString(chunk->data.size).value_or("");
        } else if (chunk->id == "ICOP") {
            out.copyright = sub.readFixedString(chunk->data.size).value_or("");
        } else if (chunk->id == "ICMT") {
            out.comment = sub.readFixedString(chunk->data.size).value_or("");
        } else if (chunk->id == "ISFT") {
            out.tools = sub.readFixedString(chunk->data.size).value_or("");
        }
        // ifil, IENG, ICRD y otros sub-chunks INFO reconocidos por la
        // spec pero no mapeados a SoundFontMetadata se ignoran
        // deliberadamente (no forman parte del modelo en esta fase).
    }
    return true;
}

bool parseSdtaChunk(const ByteSpan& listData, std::vector<std::int16_t>& out) {
    SafeByteReader reader(listData);
    while (!reader.atEnd()) {
        auto chunk = readChunk(reader);
        if (!chunk) {
            return false;
        }
        if (chunk->id == "smpl") {
            if (chunk->data.size % 2 != 0) {
                return false;
            }
            std::size_t frameCount = chunk->data.size / 2;
            if (frameCount > kMaxReasonableRecordCount) {
                return false;
            }
            out.resize(frameCount);
            SafeByteReader smplReader(chunk->data);
            for (std::size_t i = 0; i < frameCount; ++i) {
                auto sample = smplReader.readI16LE();
                if (!sample) {
                    return false;
                }
                out[i] = *sample;
            }
        }
        // "sm24" (bits bajos de audio de 24 bits) se ignora en esta
        // fase: el modelo solo soporta 16 bits por ahora.
    }
    return true;
}

/// Busca dentro de `listData` (contenido de LIST "pdta") el sub-chunk
/// con FourCC `id` y devuelve su payload. nullopt si no existe o si el
/// recorrido de chunks está corrupto.
std::optional<ByteSpan> findPdtaSubchunk(const ByteSpan& listData, const std::string& id) {
    SafeByteReader reader(listData);
    while (!reader.atEnd()) {
        auto chunk = readChunk(reader);
        if (!chunk) {
            return std::nullopt;
        }
        if (chunk->id == id) {
            return chunk->data;
        }
    }
    return std::nullopt;
}

std::optional<std::vector<RawPresetHeader>> parsePhdr(const ByteSpan& data) {
    auto count = recordCountOf(data, kPhdrRecordSize);
    if (!count || *count < 1) {
        return std::nullopt;
    }
    std::vector<RawPresetHeader> out;
    out.reserve(*count);
    SafeByteReader reader(data);
    for (std::size_t i = 0; i < *count; ++i) {
        RawPresetHeader r;
        auto name = reader.readFixedString(20);
        auto presetNum = reader.readU16LE();
        auto bank = reader.readU16LE();
        auto bagIndex = reader.readU16LE();
        auto library = reader.readU32LE();
        auto genre = reader.readU32LE();
        auto morphology = reader.readU32LE();
        if (!name || !presetNum || !bank || !bagIndex || !library || !genre || !morphology) {
            return std::nullopt;
        }
        r.name = *name;
        r.presetNumber = *presetNum;
        r.bank = *bank;
        r.bagIndex = *bagIndex;
        out.push_back(std::move(r));
    }
    return out;
}

std::optional<std::vector<RawInstHeader>> parseInst(const ByteSpan& data) {
    auto count = recordCountOf(data, kInstRecordSize);
    if (!count || *count < 1) {
        return std::nullopt;
    }
    std::vector<RawInstHeader> out;
    out.reserve(*count);
    SafeByteReader reader(data);
    for (std::size_t i = 0; i < *count; ++i) {
        auto name = reader.readFixedString(20);
        auto bagIndex = reader.readU16LE();
        if (!name || !bagIndex) {
            return std::nullopt;
        }
        out.push_back(RawInstHeader{*name, *bagIndex});
    }
    return out;
}

std::optional<std::vector<RawBag>> parseBag(const ByteSpan& data) {
    auto count = recordCountOf(data, kPbagRecordSize);
    if (!count || *count < 1) {
        return std::nullopt;
    }
    std::vector<RawBag> out;
    out.reserve(*count);
    SafeByteReader reader(data);
    for (std::size_t i = 0; i < *count; ++i) {
        auto gen = reader.readU16LE();
        auto mod = reader.readU16LE();
        if (!gen || !mod) {
            return std::nullopt;
        }
        out.push_back(RawBag{*gen, *mod});
    }
    return out;
}

std::optional<std::vector<RawGen>> parseGen(const ByteSpan& data) {
    auto count = recordCountOf(data, kPgenRecordSize);
    if (!count || *count < 1) {
        return std::nullopt;
    }
    std::vector<RawGen> out;
    out.reserve(*count);
    SafeByteReader reader(data);
    for (std::size_t i = 0; i < *count; ++i) {
        auto oper = reader.readU16LE();
        auto amount = reader.readI16LE();
        if (!oper || !amount) {
            return std::nullopt;
        }
        out.push_back(RawGen{*oper, *amount});
    }
    return out;
}

std::optional<std::vector<RawMod>> parseMod(const ByteSpan& data) {
    auto count = recordCountOf(data, kPmodRecordSize);
    if (!count || *count < 1) {
        return std::nullopt;
    }
    std::vector<RawMod> out;
    out.reserve(*count);
    SafeByteReader reader(data);
    for (std::size_t i = 0; i < *count; ++i) {
        auto src = reader.readU16LE();
        auto dest = reader.readU16LE();
        auto amount = reader.readI16LE();
        auto amtSrc = reader.readU16LE();
        auto transform = reader.readU16LE();
        if (!src || !dest || !amount || !amtSrc || !transform) {
            return std::nullopt;
        }
        out.push_back(RawMod{*src, *dest, *amount, *amtSrc, *transform});
    }
    return out;
}

std::optional<std::vector<SampleHeader>> parseShdr(const ByteSpan& data) {
    auto count = recordCountOf(data, kShdrRecordSize);
    if (!count || *count < 1) {
        return std::nullopt;
    }
    std::vector<SampleHeader> out;
    out.reserve(*count - 1);
    SafeByteReader reader(data);
    for (std::size_t i = 0; i < *count; ++i) {
        auto name = reader.readFixedString(20);
        auto start = reader.readU32LE();
        auto end = reader.readU32LE();
        auto loopStart = reader.readU32LE();
        auto loopEnd = reader.readU32LE();
        auto sampleRate = reader.readU32LE();
        auto originalPitch = reader.readU8();
        auto pitchCorrection = reader.readI8();
        auto sampleLink = reader.readU16LE();
        auto sampleType = reader.readU16LE();
        if (!name || !start || !end || !loopStart || !loopEnd || !sampleRate ||
            !originalPitch || !pitchCorrection || !sampleLink || !sampleType) {
            return std::nullopt;
        }
        if (i + 1 == *count) {
            break;  // último registro es el sentinel "EOS", se descarta
        }
        SampleHeader h;
        h.name = *name;
        h.startOffset = *start;
        h.endOffset = *end;
        h.loopStart = *loopStart;
        h.loopEnd = *loopEnd;
        h.sampleRate = *sampleRate;
        h.originalPitch = *originalPitch;
        h.pitchCorrection = *pitchCorrection;
        // Preservados tal cual, sin interpretar (ver comentario en
        // SampleHeader, model.hpp): no se pierden silenciosamente.
        h.sampleLink = *sampleLink;
        h.sampleType = *sampleType;
        out.push_back(std::move(h));
    }
    return out;
}

/// Construye las zonas de UN preset/instrumento a partir del rango de
/// bags [bagStart, bagEnd) y los arrays completos de bag/gen/mod.
/// Devuelve nullopt ante cualquier índice fuera de rango (dato no
/// confiable — nunca se accede fuera de los vectores).
std::optional<std::vector<Zone>> buildZones(const std::vector<RawBag>& bags,
                                             const std::vector<RawGen>& gens,
                                             const std::vector<RawMod>& mods,
                                             std::uint16_t bagStart, std::uint16_t bagEnd) {
    if (bagStart > bagEnd || bagEnd >= bags.size()) {
        return std::nullopt;
    }
    std::vector<Zone> zones;
    for (std::uint16_t j = bagStart; j < bagEnd; ++j) {
        const RawBag& current = bags[j];
        const RawBag& next = bags[j + 1];  // j+1 <= bagEnd < bags.size(), seguro

        if (current.genIndex > next.genIndex || next.genIndex > gens.size()) {
            return std::nullopt;
        }
        if (current.modIndex > next.modIndex || next.modIndex > mods.size()) {
            return std::nullopt;
        }

        Zone zone;
        for (std::uint16_t g = current.genIndex; g < next.genIndex; ++g) {
            zone.generators.push_back(Generator{gens[g].oper, gens[g].amount});
        }
        for (std::uint16_t m = current.modIndex; m < next.modIndex; ++m) {
            const RawMod& rm = mods[m];
            zone.modulators.push_back(Modulator{rm.sourceOper, rm.destinationOper, rm.amount,
                                                 rm.amountSourceOper, rm.transformOper});
        }
        zones.push_back(std::move(zone));
    }
    return zones;
}

class RiffSf2Parser final : public Sf2Parser {
public:
    Result<SoundFontModel> parse(const ByteSpan& data) noexcept override {
        SafeByteReader reader(data);

        auto riffId = reader.readFourCC();
        auto riffSize = reader.readU32LE();
        auto formType = reader.readFourCC();
        if (!riffId || *riffId != "RIFF" || !riffSize || !formType || *formType != "sfbk") {
            return fail(ErrorCode::MalformedInput,
                        "Cabecera RIFF/sfbk ausente o inválida — no es un archivo SF2.");
        }
        if (static_cast<std::uint64_t>(*riffSize) + 8ULL > data.size) {
            return fail(ErrorCode::MalformedInput,
                        "El tamaño RIFF declarado excede el tamaño real del buffer.");
        }

        std::optional<ByteSpan> infoData, sdtaData, pdtaData;

        while (!reader.atEnd()) {
            auto top = readChunk(reader);
            if (!top) {
                break;  // fin del contenido útil (padding/truncamiento no fatal aquí)
            }
            if (top->id != "LIST" || top->data.size < 4) {
                continue;  // chunk desconocido/extensión: se ignora, no es fatal
            }
            SafeByteReader listReader(top->data);
            auto listType = listReader.readFourCC();
            if (!listType) {
                continue;
            }
            ByteSpan rest{top->data.data + 4, top->data.size - 4};
            if (*listType == "INFO") {
                infoData = rest;
            } else if (*listType == "sdta") {
                sdtaData = rest;
            } else if (*listType == "pdta") {
                pdtaData = rest;
            }
            // Otros LIST desconocidos se ignoran (extensiones futuras).
        }

        if (!infoData || !sdtaData || !pdtaData) {
            return fail(ErrorCode::MalformedInput,
                        "Faltan uno o más chunks obligatorios (INFO/sdta/pdta).");
        }

        SoundFontModel model;

        if (!parseInfoChunk(*infoData, model.metadata)) {
            return fail(ErrorCode::MalformedInput, "Chunk INFO corrupto.");
        }
        if (!parseSdtaChunk(*sdtaData, model.sampleData)) {
            return fail(ErrorCode::MalformedInput, "Chunk sdta/smpl corrupto.");
        }

        auto phdrData = findPdtaSubchunk(*pdtaData, "phdr");
        auto pbagData = findPdtaSubchunk(*pdtaData, "pbag");
        auto pmodData = findPdtaSubchunk(*pdtaData, "pmod");
        auto pgenData = findPdtaSubchunk(*pdtaData, "pgen");
        auto instData = findPdtaSubchunk(*pdtaData, "inst");
        auto ibagData = findPdtaSubchunk(*pdtaData, "ibag");
        auto imodData = findPdtaSubchunk(*pdtaData, "imod");
        auto igenData = findPdtaSubchunk(*pdtaData, "igen");
        auto shdrData = findPdtaSubchunk(*pdtaData, "shdr");
        if (!phdrData || !pbagData || !pmodData || !pgenData || !instData || !ibagData ||
            !imodData || !igenData || !shdrData) {
            return fail(ErrorCode::MalformedInput,
                        "Faltan sub-chunks obligatorios dentro de pdta.");
        }

        auto phdr = parsePhdr(*phdrData);
        auto pbag = parseBag(*pbagData);
        auto pmod = parseMod(*pmodData);
        auto pgen = parseGen(*pgenData);
        auto inst = parseInst(*instData);
        auto ibag = parseBag(*ibagData);
        auto imod = parseMod(*imodData);
        auto igen = parseGen(*igenData);
        auto shdr = parseShdr(*shdrData);
        if (!phdr || !pbag || !pmod || !pgen || !inst || !ibag || !imod || !igen || !shdr) {
            return fail(ErrorCode::MalformedInput,
                        "Registros de pdta con tamaño inconsistente o vacíos.");
        }

        model.samples = std::move(*shdr);

        // Instrumentos (excluye el registro terminal "EOI").
        for (std::size_t i = 0; i + 1 < inst->size(); ++i) {
            std::uint16_t bagStart = (*inst)[i].bagIndex;
            std::uint16_t bagEnd = (*inst)[i + 1].bagIndex;
            auto zones = buildZones(*ibag, *igen, *imod, bagStart, bagEnd);
            if (!zones) {
                return fail(ErrorCode::MalformedInput,
                            "Índices de zona de instrumento fuera de rango.");
            }
            Instrument instrument;
            instrument.name = (*inst)[i].name;
            instrument.zones = std::move(*zones);
            model.instruments.push_back(std::move(instrument));
        }

        // Presets (excluye el registro terminal "EOP").
        for (std::size_t i = 0; i + 1 < phdr->size(); ++i) {
            std::uint16_t bagStart = (*phdr)[i].bagIndex;
            std::uint16_t bagEnd = (*phdr)[i + 1].bagIndex;
            auto zones = buildZones(*pbag, *pgen, *pmod, bagStart, bagEnd);
            if (!zones) {
                return fail(ErrorCode::MalformedInput,
                            "Índices de zona de preset fuera de rango.");
            }
            Preset preset;
            preset.name = (*phdr)[i].name;
            preset.presetNumber = (*phdr)[i].presetNumber;
            preset.bank = (*phdr)[i].bank;
            preset.zones = std::move(*zones);
            model.presets.push_back(std::move(preset));
        }

        return Result<SoundFontModel>::ok(std::move(model));
    }
};

}  // namespace

std::unique_ptr<Sf2Parser> createDefaultSf2Parser() {
    return std::make_unique<RiffSf2Parser>();
}

}  // namespace olysf2sampler::soundfont
