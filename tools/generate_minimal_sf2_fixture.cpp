// Utilidad de una sola vez (no forma parte del build de producto):
// genera tests/fixtures/valid/minimal.sf2 usando el Sf2Writer real,
// para tener un fixture SF2 auténtico (no hex hecho a mano) sin
// depender de acceso de red para descargar uno de terceros.
//
// Uso: se compila y ejecuta manualmente cuando se necesite regenerar
// el fixture; no se integra en CMake/CI.

#include <cstdio>
#include <vector>

#include "olysf2sampler/soundfont/model.hpp"
#include "olysf2sampler/soundfont/writer.hpp"

using namespace olysf2sampler::soundfont;

int main() {
    SoundFontModel model;
    model.metadata.soundEngine = "EMU8000";
    model.metadata.bankName = "OlySf2 Sampler Minimal Fixture";
    model.metadata.productName = "OlySf2 Sampler";
    model.metadata.copyright = "YeiViKas Digital Company";
    model.metadata.comment =
        "Fixture minimo generado por tools/generate_minimal_sf2_fixture.cpp";
    model.metadata.tools = "OlySf2SamplerWriter";

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
    instrumentZone.generators.push_back(Generator{GeneratorOperator::kGeneratorSampleID, 0});
    instrument.zones.push_back(instrumentZone);
    model.instruments.push_back(instrument);

    Preset preset;
    preset.name = "TestPreset";
    preset.presetNumber = 0;
    preset.bank = 0;
    Zone presetZone;
    presetZone.generators.push_back(Generator{GeneratorOperator::kGeneratorInstrument, 0});
    preset.zones.push_back(presetZone);
    model.presets.push_back(preset);

    auto writer = createDefaultSf2Writer();
    auto result = writer->write(model);
    if (!result.isOk()) {
        std::fprintf(stderr, "Fallo al escribir el modelo.\n");
        return 1;
    }

    const std::vector<std::uint8_t>& bytes = result.value();
    FILE* f = std::fopen("tests/fixtures/valid/minimal.sf2", "wb");
    if (!f) {
        std::fprintf(stderr, "No se pudo abrir el archivo de salida.\n");
        return 1;
    }
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    std::printf("Escrito tests/fixtures/valid/minimal.sf2 (%zu bytes)\n", bytes.size());
    return 0;
}
