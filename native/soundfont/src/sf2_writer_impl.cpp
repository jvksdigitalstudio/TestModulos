#include "olysf2sampler/soundfont/writer.hpp"

#include <cstring>
#include <optional>
#include <string>

#include "sf2_binary_format.hpp"

namespace olysf2sampler::soundfont {

namespace {

using core::Error;
using core::ErrorCode;
using core::Result;

/// Escritor de bytes little-endian con padding automático de chunks
/// RIFF a tamaño par. Contraparte de detail::SafeByteReader.
class ByteWriter {
public:
    void writeU8(std::uint8_t v) { bytes_.push_back(v); }
    void writeI8(std::int8_t v) { bytes_.push_back(static_cast<std::uint8_t>(v)); }

    void writeU16LE(std::uint16_t v) {
        bytes_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    }
    void writeI16LE(std::int16_t v) { writeU16LE(static_cast<std::uint16_t>(v)); }

    void writeU32LE(std::uint32_t v) {
        bytes_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        bytes_.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }

    void writeFourCC(const char (&fourcc)[5]) {
        bytes_.insert(bytes_.end(), fourcc, fourcc + 4);
    }

    /// Escribe `str` truncado/rellenado a exactamente `len` bytes
    /// (relleno con '\0'), como exige la spec para nombres de longitud
    /// fija (achPresetName, achInstName, achSampleName).
    void writeFixedString(const std::string& str, std::size_t len) {
        std::size_t copyLen = str.size() < len ? str.size() : len;
        bytes_.insert(bytes_.end(), str.begin(), str.begin() + static_cast<long>(copyLen));
        for (std::size_t i = copyLen; i < len; ++i) {
            bytes_.push_back(0);
        }
    }

    void appendRaw(const std::vector<std::uint8_t>& other) {
        bytes_.insert(bytes_.end(), other.begin(), other.end());
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(bytes_); }

private:
    std::vector<std::uint8_t> bytes_;
};

/// Envuelve `payload` como un chunk RIFF completo (id + size + data +
/// padding si `payload.size()` es impar) y lo agrega a `dest`.
void writeChunk(ByteWriter& dest, const char (&id)[5], const std::vector<std::uint8_t>& payload) {
    dest.writeFourCC(id);
    dest.writeU32LE(static_cast<std::uint32_t>(payload.size()));
    dest.appendRaw(payload);
    if (payload.size() % 2 == 1) {
        dest.writeU8(0);
    }
}

/// Escribe un sub-chunk INFO de texto null-terminated (isng/INAM/IPRD/
/// ICOP/ICMT/ISFT). Se omite por completo si `value` está vacío y
/// `required` es false (la mayoría de estos campos son opcionales en
/// la spec; INAM es el único obligatorio, por eso ese llamador pasa
/// required=true con un valor por defecto no vacío).
void writeInfoTextChunk(ByteWriter& dest, const char (&id)[5], const std::string& value,
                        bool required) {
    if (value.empty() && !required) {
        return;
    }
    ByteWriter payload;
    const std::string& text = value.empty() ? std::string("Untitled") : value;
    payload.appendRaw(std::vector<std::uint8_t>(text.begin(), text.end()));
    payload.writeU8(0);  // terminador null exigido por la spec
    writeChunk(dest, id, payload.take());
}

std::vector<std::uint8_t> buildInfoList(const SoundFontMetadata& metadata) {
    ByteWriter listPayload;
    listPayload.writeFourCC("INFO");

    ByteWriter ifil;
    ifil.writeU16LE(2);  // wMajor — versión SF2 declarada por este writer
    ifil.writeU16LE(1);  // wMinor
    writeChunk(listPayload, "ifil", ifil.take());

    writeInfoTextChunk(listPayload, "isng",
                       metadata.soundEngine.empty() ? "EMU8000" : metadata.soundEngine, true);
    writeInfoTextChunk(listPayload, "INAM", metadata.bankName, true);
    writeInfoTextChunk(listPayload, "IPRD", metadata.productName, false);
    writeInfoTextChunk(listPayload, "ICOP", metadata.copyright, false);
    writeInfoTextChunk(listPayload, "ICMT", metadata.comment, false);
    writeInfoTextChunk(listPayload, "ISFT", metadata.tools, false);

    ByteWriter out;
    writeChunk(out, "LIST", listPayload.take());
    return out.take();
}

std::vector<std::uint8_t> buildSdtaList(const std::vector<std::int16_t>& sampleData) {
    ByteWriter listPayload;
    listPayload.writeFourCC("sdta");

    ByteWriter smpl;
    for (std::int16_t s : sampleData) {
        smpl.writeI16LE(s);
    }
    writeChunk(listPayload, "smpl", smpl.take());

    ByteWriter out;
    writeChunk(out, "LIST", listPayload.take());
    return out.take();
}

struct PdtaBuffers {
    ByteWriter phdr, pbag, pmod, pgen;
    ByteWriter inst, ibag, imod, igen;
    ByteWriter shdr;
};

/// Límite duro derivado del formato SF2: los índices de bag/gen/mod
/// son campos de 16 bits sin signo. Superarlo NO se resuelve con
/// wrapping silencioso (prohibido explícitamente por Fase A.1 §8):
/// se detecta aquí y se propaga como ResourceLimitExceeded.
constexpr std::uint32_t kMaxU16Index = 0xFFFFu;

/// Añade una zona a los buffers de bag/gen/mod, avanzando los cursores
/// (mantenidos en uint32_t para poder detectar overflow ANTES de
/// truncar a los uint16_t reales del formato). Devuelve false si
/// escribir esta zona haría que algún cursor superase kMaxU16Index.
[[nodiscard]] bool appendZone(ByteWriter& bagChunk, ByteWriter& genChunk, ByteWriter& modChunk,
                              std::uint32_t& genCursor, std::uint32_t& modCursor,
                              const Zone& zone) {
    if (genCursor > kMaxU16Index || modCursor > kMaxU16Index) {
        return false;
    }
    bagChunk.writeU16LE(static_cast<std::uint16_t>(genCursor));
    bagChunk.writeU16LE(static_cast<std::uint16_t>(modCursor));
    for (const Generator& g : zone.generators) {
        if (genCursor >= kMaxU16Index) {
            return false;
        }
        genChunk.writeU16LE(g.type);
        genChunk.writeI16LE(g.value);
        ++genCursor;
    }
    for (const Modulator& m : zone.modulators) {
        if (modCursor >= kMaxU16Index) {
            return false;
        }
        modChunk.writeU16LE(m.sourceOper);
        modChunk.writeU16LE(m.destinationOper);
        modChunk.writeI16LE(m.amount);
        modChunk.writeU16LE(m.amountSourceOper);
        modChunk.writeU16LE(m.transformOper);
        ++modCursor;
    }
    return true;
}

[[nodiscard]] bool buildPresets(PdtaBuffers& out, const std::vector<Preset>& presets) {
    std::uint32_t bagCursor = 0;
    std::uint32_t genCursor = 0;
    std::uint32_t modCursor = 0;

    for (const Preset& preset : presets) {
        if (bagCursor > kMaxU16Index) {
            return false;
        }
        out.phdr.writeFixedString(preset.name, 20);
        out.phdr.writeU16LE(preset.presetNumber);
        out.phdr.writeU16LE(preset.bank);
        out.phdr.writeU16LE(static_cast<std::uint16_t>(bagCursor));
        out.phdr.writeU32LE(0);  // dwLibrary
        out.phdr.writeU32LE(0);  // dwGenre
        out.phdr.writeU32LE(0);  // dwMorphology

        for (const Zone& zone : preset.zones) {
            if (!appendZone(out.pbag, out.pgen, out.pmod, genCursor, modCursor, zone)) {
                return false;
            }
            if (bagCursor >= kMaxU16Index) {
                return false;
            }
            ++bagCursor;
        }
    }
    if (bagCursor > kMaxU16Index) {
        return false;
    }
    // Registros terminales (sentinel), exigidos por la spec.
    out.phdr.writeFixedString("EOP", 20);
    out.phdr.writeU16LE(0);
    out.phdr.writeU16LE(0);
    out.phdr.writeU16LE(static_cast<std::uint16_t>(bagCursor));
    out.phdr.writeU32LE(0);
    out.phdr.writeU32LE(0);
    out.phdr.writeU32LE(0);

    out.pbag.writeU16LE(static_cast<std::uint16_t>(genCursor));
    out.pbag.writeU16LE(static_cast<std::uint16_t>(modCursor));
    out.pgen.writeU16LE(0);
    out.pgen.writeI16LE(0);
    out.pmod.writeU16LE(0);
    out.pmod.writeU16LE(0);
    out.pmod.writeI16LE(0);
    out.pmod.writeU16LE(0);
    out.pmod.writeU16LE(0);
    return true;
}

[[nodiscard]] bool buildInstruments(PdtaBuffers& out, const std::vector<Instrument>& instruments) {
    std::uint32_t bagCursor = 0;
    std::uint32_t genCursor = 0;
    std::uint32_t modCursor = 0;

    for (const Instrument& instrument : instruments) {
        if (bagCursor > kMaxU16Index) {
            return false;
        }
        out.inst.writeFixedString(instrument.name, 20);
        out.inst.writeU16LE(static_cast<std::uint16_t>(bagCursor));

        for (const Zone& zone : instrument.zones) {
            if (!appendZone(out.ibag, out.igen, out.imod, genCursor, modCursor, zone)) {
                return false;
            }
            if (bagCursor >= kMaxU16Index) {
                return false;
            }
            ++bagCursor;
        }
    }
    if (bagCursor > kMaxU16Index) {
        return false;
    }
    out.inst.writeFixedString("EOI", 20);
    out.inst.writeU16LE(static_cast<std::uint16_t>(bagCursor));

    out.ibag.writeU16LE(static_cast<std::uint16_t>(genCursor));
    out.ibag.writeU16LE(static_cast<std::uint16_t>(modCursor));
    out.igen.writeU16LE(0);
    out.igen.writeI16LE(0);
    out.imod.writeU16LE(0);
    out.imod.writeU16LE(0);
    out.imod.writeI16LE(0);
    out.imod.writeU16LE(0);
    out.imod.writeU16LE(0);
    return true;
}

void buildSamples(PdtaBuffers& out, const std::vector<SampleHeader>& samples) {
    for (const SampleHeader& s : samples) {
        out.shdr.writeFixedString(s.name, 20);
        out.shdr.writeU32LE(s.startOffset);
        out.shdr.writeU32LE(s.endOffset);
        out.shdr.writeU32LE(s.loopStart);
        out.shdr.writeU32LE(s.loopEnd);
        out.shdr.writeU32LE(s.sampleRate);
        out.shdr.writeU8(s.originalPitch);
        out.shdr.writeI8(s.pitchCorrection);
        out.shdr.writeU16LE(s.sampleLink);
        out.shdr.writeU16LE(s.sampleType);
    }
    out.shdr.writeFixedString("EOS", 20);
    out.shdr.writeU32LE(0);
    out.shdr.writeU32LE(0);
    out.shdr.writeU32LE(0);
    out.shdr.writeU32LE(0);
    out.shdr.writeU32LE(0);
    out.shdr.writeU8(0);
    out.shdr.writeI8(0);
    out.shdr.writeU16LE(0);
    out.shdr.writeU16LE(0);
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> buildPdtaList(const SoundFontModel& model) {
    PdtaBuffers buf;
    if (!buildPresets(buf, model.presets)) {
        return std::nullopt;
    }
    if (!buildInstruments(buf, model.instruments)) {
        return std::nullopt;
    }
    buildSamples(buf, model.samples);

    ByteWriter listPayload;
    listPayload.writeFourCC("pdta");
    writeChunk(listPayload, "phdr", buf.phdr.take());
    writeChunk(listPayload, "pbag", buf.pbag.take());
    writeChunk(listPayload, "pmod", buf.pmod.take());
    writeChunk(listPayload, "pgen", buf.pgen.take());
    writeChunk(listPayload, "inst", buf.inst.take());
    writeChunk(listPayload, "ibag", buf.ibag.take());
    writeChunk(listPayload, "imod", buf.imod.take());
    writeChunk(listPayload, "igen", buf.igen.take());
    writeChunk(listPayload, "shdr", buf.shdr.take());

    ByteWriter out;
    writeChunk(out, "LIST", listPayload.take());
    return out.take();
}

class RiffSf2Writer final : public Sf2Writer {
public:
    // NO noexcept (fix §9, mismo razonamiento que el parser): escribir
    // un SF2 aloja memoria dinámicamente (ByteWriter usa std::vector
    // internamente) en una ruta offline, no realtime. Antes,
    // `write()` prometía noexcept sin poder garantizarlo — una
    // excepción real ahí terminaba en std::terminate() inmediato.
    Result<std::vector<std::uint8_t>> write(const SoundFontModel& model) override {
        try {
            return writeImpl(model);
        } catch (const std::bad_alloc&) {
            return Result<std::vector<std::uint8_t>>::fail(
                Error{ErrorCode::OutOfMemory,
                      "Sin memoria suficiente para serializar este SoundFont."});
        } catch (const std::exception& e) {
            return Result<std::vector<std::uint8_t>>::fail(
                Error{ErrorCode::Unknown,
                      std::string("Excepción inesperada durante la escritura: ") + e.what()});
        }
    }

private:
    Result<std::vector<std::uint8_t>> writeImpl(const SoundFontModel& model) {
        auto pdtaList = buildPdtaList(model);
        if (!pdtaList) {
            return Result<std::vector<std::uint8_t>>::fail(Error{
                ErrorCode::ResourceLimitExceeded,
                "El modelo excede los límites de índice de 16 bits del formato SF2 "
                "(demasiadas zonas/generadores/moduladores en presets o instrumentos); "
                "escritura abortada en vez de generar un archivo con índices truncados "
                "silenciosamente."});
        }
        ByteWriter riffPayload;
        riffPayload.writeFourCC("sfbk");
        riffPayload.appendRaw(buildInfoList(model.metadata));
        riffPayload.appendRaw(buildSdtaList(model.sampleData));
        riffPayload.appendRaw(*pdtaList);

        ByteWriter out;
        writeChunk(out, "RIFF", riffPayload.take());
        return Result<std::vector<std::uint8_t>>::ok(out.take());
    }
};

}  // namespace

std::unique_ptr<Sf2Writer> createDefaultSf2Writer() {
    return std::make_unique<RiffSf2Writer>();
}

}  // namespace olysf2sampler::soundfont
