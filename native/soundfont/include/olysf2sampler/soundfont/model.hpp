#pragma once
// native/soundfont/model — estructuras de datos SF2.
// Responsabilidad única: representar en memoria un SoundFont
// (Header/Samples/Instruments/Presets/Zones/Generators/Modulators).
// No parsea bytes ni escribe archivos (eso es parser/ y writer/).
//
// Mapeo a la estructura RIFF real de un archivo .sf2 (para quien
// implemente el parser/writer en Fase B — este modelo es
// deliberadamente independiente de la forma binaria RIFF, pero cada
// tipo de aquí corresponde a un chunk/sub-chunk concreto de la spec):
//
//   RIFF/sfbk
//   ├── LIST "INFO"   -> SoundFontMetadata (ifil/isng/INAM/ICOP/ICMT/ISFT)
//   ├── LIST "sdta"   -> datos crudos de audio de todos los samples
//   │                    (SampleHeader::startOffset/endOffset indexan aquí)
//   └── LIST "pdta"   -> phdr/pbag/pgen/pmod  -> Preset (+ sus Zone)
//                        inst/ibag/igen/imod  -> Instrument (+ sus Zone)
//                        shdr                  -> SampleHeader
//
// Se fijan aquí los límites conceptuales del modelo; la implementación
// del parser/writer conforme a la especificación SF2 completa
// (incluyendo el recorrido real de estos chunks RIFF) llega en la
// fase "SoundFont Core" del roadmap — ver docs/architecture/SECURITY.md
// para las reglas de manejo de datos no confiables que esa
// implementación deberá seguir.

#include <cstdint>
#include <string>
#include <vector>

namespace olysf2sampler::soundfont {

struct Generator {
    std::uint16_t type{0};
    std::int16_t value{0};
};

struct Modulator {
    std::uint16_t sourceOper{0};
    std::uint16_t destinationOper{0};
    std::int16_t amount{0};
    std::uint16_t amountSourceOper{0};
    std::uint16_t transformOper{0};
};

struct Zone {
    std::vector<Generator> generators;
    std::vector<Modulator> modulators;
};

struct SampleHeader {
    std::string name;
    std::uint32_t startOffset{0};
    std::uint32_t endOffset{0};
    std::uint32_t loopStart{0};
    std::uint32_t loopEnd{0};
    std::uint32_t sampleRate{44100};
    std::uint8_t originalPitch{60};
    std::int8_t pitchCorrection{0};

    /// wSampleLink (shdr): índice de otro SampleHeader relacionado
    /// (canal L/R de un sample estéreo enlazado, ROM sample linking).
    /// PRESERVADO tal cual desde el parser y reescrito tal cual por el
    /// writer (estrategia "preservar" de Fase A.1 §6) — su semántica
    /// (resolución de pares estéreo / ROM linking) todavía NO se
    /// interpreta en ninguna capa superior de este módulo.
    std::uint16_t sampleLink{0};

    /// sfSampleType (shdr): bitfield SF2 (monoSample=1, rightSample=2,
    /// leftSample=4, linkedSample=8, RomMonoSample=0x8001, etc).
    /// Mismo estado que sampleLink: preservado, no interpretado
    /// todavía. Un valor no reconocido NO se rechaza ni se trunca —
    /// se conserva como dato opaco para no perder información de
    /// SoundFonts reales que sí usan estos campos.
    std::uint16_t sampleType{1};  // 1 = monoSample, valor por defecto conservador
};

struct Instrument {
    std::string name;
    std::vector<Zone> zones;
};

struct Preset {
    std::string name;
    std::uint16_t presetNumber{0};
    std::uint16_t bank{0};
    std::vector<Zone> zones;
};

struct SoundFontMetadata {
    std::string soundEngine;
    std::string bankName;
    std::string productName;
    std::string copyright;
    std::string comment;
    std::string tools;
};

/// Modelo raíz en memoria de un SoundFont 2 completo.
struct SoundFontModel {
    SoundFontMetadata metadata;
    std::vector<SampleHeader> samples;
    std::vector<Instrument> instruments;
    std::vector<Preset> presets;

    /// PCM de 16 bits con signo de TODOS los samples, concatenado en un
    /// único buffer (corresponde al chunk "smpl" dentro de LIST "sdta").
    /// `SampleHeader::startOffset`/`endOffset`/`loopStart`/`loopEnd` son
    /// índices dentro de este vector (no offsets en bytes), tal como
    /// los define la especificación SF2.
    std::vector<std::int16_t> sampleData;
};

/// Operadores de generador relevantes para navegar referencias cruzadas
/// dentro del modelo (Preset -> Instrument, Instrument -> Sample). El
/// resto de operadores de generador (~58 en total en la spec SF2) se
/// preservan como pares (type, value) genéricos en `Generator` sin
/// necesitar una constante nombrada para cada uno — solo se nombran
/// aquí los que el parser/writer/validator necesitan interpretar
/// estructuralmente para construir/verificar el árbol de zonas.
///
/// NOTA: estos valores se recuerdan de la especificación SoundFont 2.04
/// y deberían contrastarse contra el documento oficial antes de usarse
/// para verificar compatibilidad con archivos SF2 de terceros en
/// producción.
enum GeneratorOperator : std::uint16_t {
    kGeneratorInstrument = 41,  // en una zona de Preset: índice de Instrument
    kGeneratorSampleID = 53,    // en una zona de Instrument: índice de SampleHeader
};

}  // namespace olysf2sampler::soundfont
