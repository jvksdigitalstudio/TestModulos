// Round-trip real del núcleo SoundFont: construye un SoundFontModel
// mínimo en memoria, lo serializa con Sf2Writer, lo vuelve a parsear
// con Sf2Parser, y verifica que el resultado sea equivalente al
// original. También prueba Sf2Validator sobre el modelo parseado, y
// dos casos de entrada malformada (RIFF corrupto / chunk truncado).
//
// Esto NO es un smoke test de "compila y no explota": compara valores
// reales campo a campo tras una vuelta completa de codificación.

#include <cassert>
#include <cstdio>
#include <cstring>

#include "olysf2sampler/soundfont/model.hpp"
#include "olysf2sampler/soundfont/parser.hpp"
#include "olysf2sampler/soundfont/validator.hpp"
#include "olysf2sampler/soundfont/writer.hpp"

using namespace olysf2sampler::soundfont;

namespace {

SoundFontModel buildMinimalModel() {
    SoundFontModel model;
    model.metadata.soundEngine = "EMU8000";
    model.metadata.bankName = "OlySf2 Sampler Test Bank";
    model.metadata.productName = "OlySf2 Sampler";
    model.metadata.copyright = "YeiViKas Digital Company";
    model.metadata.comment = "Fixture de round-trip generada por tests/soundfont.";
    model.metadata.tools = "OlySf2SamplerWriter";

    // 8 frames de PCM de prueba (no es audio real, solo datos
    // reconocibles para verificar que sobreviven el round-trip).
    model.sampleData = {0, 1000, 2000, 3000, 2000, 1000, 0, -1000};

    SampleHeader sample;
    sample.name = "TestSine";
    sample.startOffset = 0;
    sample.endOffset = 8;
    sample.loopStart = 2;
    sample.loopEnd = 6;
    sample.sampleRate = 44100;
    sample.originalPitch = 60;
    sample.pitchCorrection = 0;
    model.samples.push_back(sample);

    Instrument instrument;
    instrument.name = "TestInstrument";
    Zone instrumentZone;
    instrumentZone.generators.push_back(
        Generator{GeneratorOperator::kGeneratorSampleID, 0});  // referencia sample #0
    instrument.zones.push_back(instrumentZone);
    model.instruments.push_back(instrument);

    Preset preset;
    preset.name = "TestPreset";
    preset.presetNumber = 0;
    preset.bank = 0;
    Zone presetZone;
    presetZone.generators.push_back(
        Generator{GeneratorOperator::kGeneratorInstrument, 0});  // referencia instrument #0
    preset.zones.push_back(presetZone);
    model.presets.push_back(preset);

    return model;
}

void testRoundTripPreservesModel() {
    SoundFontModel original = buildMinimalModel();

    auto writer = createDefaultSf2Writer();
    auto writeResult = writer->write(original);
    assert(writeResult.isOk());
    const std::vector<std::uint8_t>& bytes = writeResult.value();
    assert(!bytes.empty());

    // Cabecera RIFF/sfbk reconocible a simple vista.
    assert(bytes.size() >= 12);
    assert(std::memcmp(bytes.data(), "RIFF", 4) == 0);
    assert(std::memcmp(bytes.data() + 8, "sfbk", 4) == 0);

    auto parser = createDefaultSf2Parser();
    ByteSpan span{bytes.data(), bytes.size()};
    auto parseResult = parser->parse(span);
    assert(parseResult.isOk());
    const SoundFontModel& parsed = parseResult.value();

    assert(parsed.metadata.bankName == original.metadata.bankName);
    assert(parsed.metadata.copyright == original.metadata.copyright);

    assert(parsed.sampleData == original.sampleData);

    assert(parsed.samples.size() == 1);
    assert(parsed.samples[0].name == "TestSine");
    assert(parsed.samples[0].startOffset == 0);
    assert(parsed.samples[0].endOffset == 8);
    assert(parsed.samples[0].loopStart == 2);
    assert(parsed.samples[0].loopEnd == 6);
    assert(parsed.samples[0].sampleRate == 44100);

    assert(parsed.instruments.size() == 1);
    assert(parsed.instruments[0].name == "TestInstrument");
    assert(parsed.instruments[0].zones.size() == 1);
    assert(parsed.instruments[0].zones[0].generators.size() == 1);
    assert(parsed.instruments[0].zones[0].generators[0].type ==
           GeneratorOperator::kGeneratorSampleID);
    assert(parsed.instruments[0].zones[0].generators[0].value == 0);

    assert(parsed.presets.size() == 1);
    assert(parsed.presets[0].name == "TestPreset");
    assert(parsed.presets[0].zones.size() == 1);
    assert(parsed.presets[0].zones[0].generators[0].type ==
           GeneratorOperator::kGeneratorInstrument);

    auto validator = createDefaultSf2Validator();
    ValidationReport report = validator->validate(parsed);
    assert(report.isValid);

    std::printf("[tests/soundfont] round-trip OK (%zu bytes, %zu issues no fatales)\n",
                bytes.size(), report.issues.size());
}

void testValidatorCatchesOutOfRangeReference() {
    SoundFontModel model = buildMinimalModel();
    // Corrompe la referencia: el preset apunta al instrumento #5, que
    // no existe (solo hay 1 instrumento, índice 0).
    model.presets[0].zones[0].generators[0].value = 5;

    auto validator = createDefaultSf2Validator();
    ValidationReport report = validator->validate(model);
    assert(!report.isValid);
    bool foundIssue = false;
    for (const auto& issue : report.issues) {
        if (issue.isFatal) {
            foundIssue = true;
        }
    }
    assert(foundIssue);
    std::printf("[tests/soundfont] validator detecta referencia fuera de rango OK\n");
}

void testParserRejectsCorruptRiffHeader() {
    std::vector<std::uint8_t> garbage = {'N', 'O', 'T', 'A', 0, 0, 0, 0, 'f', 'a', 'k', 'e'};
    auto parser = createDefaultSf2Parser();
    ByteSpan span{garbage.data(), garbage.size()};
    auto result = parser->parse(span);
    assert(result.isError());
    assert(result.error().code == olysf2sampler::core::ErrorCode::MalformedInput);
    std::printf("[tests/soundfont] parser rechaza cabecera RIFF corrupta OK\n");
}

void testParserRejectsTruncatedFile() {
    SoundFontModel original = buildMinimalModel();
    auto writer = createDefaultSf2Writer();
    auto writeResult = writer->write(original);
    assert(writeResult.isOk());
    std::vector<std::uint8_t> truncated = writeResult.value();
    truncated.resize(truncated.size() / 2);  // corta el archivo a la mitad

    auto parser = createDefaultSf2Parser();
    ByteSpan span{truncated.data(), truncated.size()};
    auto result = parser->parse(span);
    assert(result.isError());
    assert(result.error().code == olysf2sampler::core::ErrorCode::MalformedInput);
    std::printf("[tests/soundfont] parser rechaza archivo truncado OK\n");
}

}  // namespace

int main() {
    testRoundTripPreservesModel();
    testValidatorCatchesOutOfRangeReference();
    testParserRejectsCorruptRiffHeader();
    testParserRejectsTruncatedFile();
    std::printf("[tests/soundfont] TODOS los tests de round-trip pasaron\n");
    return 0;
}
