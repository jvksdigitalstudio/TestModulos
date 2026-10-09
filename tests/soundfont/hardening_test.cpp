// tests/soundfont/hardening_test.cpp
//
// Regresiones específicas de Fase A.1 (consolidación/hardening):
//
// 1. wSampleLink/sfSampleType ya no se descartan silenciosamente en
//    el parser ni se hardcodean en el writer (§6: "no destruyas
//    información silenciosamente"). Antes de este fix, cualquier SF2
//    con sample linking (estéreo enlazado, ROM) perdía esos campos en
//    cuanto pasaba por un ciclo parse->write.
//
// 2. El writer detecta si un modelo excede los índices de 16 bits de
//    bag/gen/mod (demasiadas zonas/generadores/moduladores) y
//    devuelve ResourceLimitExceeded en vez de wrappear los índices
//    silenciosamente (§8: "Nunca permitas wrapping silencioso").

#include <cassert>
#include <cstdio>

#include "olysf2sampler/soundfont/model.hpp"
#include "olysf2sampler/soundfont/parser.hpp"
#include "olysf2sampler/soundfont/writer.hpp"

using namespace olysf2sampler::soundfont;

namespace {

SoundFontModel buildMinimalModelWithSampleLink() {
    SoundFontModel model;
    model.metadata.bankName = "HardeningTestBank";
    model.sampleData = {0, 100, 200, 100, 0, -100};

    SampleHeader left;
    left.name = "StereoLeft";
    left.startOffset = 0;
    left.endOffset = 3;
    left.loopStart = 0;
    left.loopEnd = 3;
    left.sampleRate = 44100;
    left.sampleLink = 1;       // apunta al índice del canal derecho
    left.sampleType = 4;       // leftSample (bitfield SF2 real)
    model.samples.push_back(left);

    SampleHeader right;
    right.name = "StereoRight";
    right.startOffset = 3;
    right.endOffset = 6;
    right.loopStart = 3;
    right.loopEnd = 6;
    right.sampleRate = 44100;
    right.sampleLink = 0;      // apunta al canal izquierdo
    right.sampleType = 2;      // rightSample
    model.samples.push_back(right);

    Instrument instrument;
    instrument.name = "StereoInstrument";
    Zone zone;
    zone.generators.push_back(Generator{GeneratorOperator::kGeneratorSampleID, 0});
    instrument.zones.push_back(zone);
    model.instruments.push_back(instrument);

    Preset preset;
    preset.name = "StereoPreset";
    Zone presetZone;
    presetZone.generators.push_back(Generator{GeneratorOperator::kGeneratorInstrument, 0});
    preset.zones.push_back(presetZone);
    model.presets.push_back(preset);

    return model;
}

void testSampleLinkAndTypeSurviveRoundTrip() {
    SoundFontModel original = buildMinimalModelWithSampleLink();

    auto writer = createDefaultSf2Writer();
    auto writeResult = writer->write(original);
    assert(writeResult.isOk());

    auto parser = createDefaultSf2Parser();
    ByteSpan span{writeResult.value().data(), writeResult.value().size()};
    auto parseResult = parser->parse(span);
    assert(parseResult.isOk());
    const SoundFontModel& parsed = parseResult.value();

    assert(parsed.samples.size() == 2);
    assert(parsed.samples[0].sampleLink == 1);
    assert(parsed.samples[0].sampleType == 4);
    assert(parsed.samples[1].sampleLink == 0);
    assert(parsed.samples[1].sampleType == 2);

    std::printf(
        "[tests/soundfont] sampleLink/sampleType sobreviven round-trip (antes se perdían) OK\n");
}

void testWriterRejectsModelExceedingU16IndexInsteadOfWrapping() {
    // Construye un preset con más de 65535 zonas para forzar overflow
    // del cursor de bag (uint16 en el formato real). No sembramos
    // 65536 zonas reales en memoria por costo del test; en su lugar
    // confiamos en que el propio writer trata esto como corrupción
    // estructural real ante cualquier exceso, así que basta con un
    // valor bien por encima del límite pero manejable: usamos el
    // generador de zonas vacías (sin generators/modulators), el costo
    // dominante es solo la escritura de 4 bytes por bag.
    SoundFontModel model;
    model.metadata.bankName = "OverflowTestBank";

    Preset preset;
    preset.name = "HugePreset";
    constexpr int kZoneCount = 70000;  // > 65535: debe disparar ResourceLimitExceeded
    preset.zones.reserve(kZoneCount);
    for (int i = 0; i < kZoneCount; ++i) {
        preset.zones.push_back(Zone{});  // zona vacía: sin generators/modulators
    }
    model.presets.push_back(std::move(preset));

    auto writer = createDefaultSf2Writer();
    auto writeResult = writer->write(model);
    assert(writeResult.isError());
    assert(writeResult.error().code == olysf2sampler::core::ErrorCode::ResourceLimitExceeded);

    std::printf(
        "[tests/soundfont] writer devuelve ResourceLimitExceeded ante overflow de índice de "
        "16 bits en vez de wrappear silenciosamente OK\n");
}

}  // namespace

int main() {
    testSampleLinkAndTypeSurviveRoundTrip();
    testWriterRejectsModelExceedingU16IndexInsteadOfWrapping();
    std::printf("[tests/soundfont] hardening_test: TODOS los tests pasaron\n");
    return 0;
}
