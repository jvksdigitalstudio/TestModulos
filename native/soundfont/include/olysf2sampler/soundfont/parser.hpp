#pragma once
// native/soundfont/parser — interpreta bytes SF2 hacia
// SoundFontModel. Responsabilidad única: parsear. No valida
// semánticamente (eso es validation/) ni escribe (eso es writer/).
//
// SEGURIDAD (ver docs/architecture/SECURITY.md): la entrada de este
// parser es SIEMPRE un archivo SF2 potencialmente ajeno/no confiable.
// La implementación real (pendiente de Fase B) debe validar cada
// offset/tamaño leído contra los límites reales del buffer antes de
// desreferenciarlo, y debe devolver `Result::fail` con
// `ErrorCode::MalformedInput` — nunca leer fuera de rango ni asumir
// que un tamaño declarado en el archivo es correcto.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "olysf2sampler/core/result.hpp"
#include "olysf2sampler/soundfont/model.hpp"

namespace olysf2sampler::soundfont {

struct ByteSpan {
    const std::uint8_t* data{nullptr};
    std::size_t size{0};
};

class Sf2Parser {
public:
    virtual ~Sf2Parser() = default;

    /// Parsea un buffer SF2 completo ya cargado en memoria. La carga
    /// desde disco/URI (y su bounds-checking de I/O) es responsabilidad
    /// de `olysf2sampler::samples::io`, no de este tipo — este método
    /// solo recibe bytes ya en memoria y debe tratarlos como no
    /// confiables (ver nota de seguridad arriba).
    /// NO noexcept: ruta offline (no realtime). Cualquier excepción
    /// real (p.ej. std::bad_alloc) es atrapada por la implementación
    /// y convertida en Result::fail — ver sf2_parser_impl.cpp. Un
    /// `noexcept` aquí sería una promesa falsa dado que el parseo
    /// aloja memoria dinámicamente en decenas de puntos (Fase A.1 §9).
    virtual olysf2sampler::core::Result<SoundFontModel> parse(const ByteSpan& data) = 0;
};

/// Construye la implementación real (RIFF/INFO/sdta/pdta) de Sf2Parser.
/// Quien consuma este módulo solo debe conocer la interfaz `Sf2Parser`
/// y esta factoría — nunca el nombre de la clase concreta, que es un
/// detalle interno de `native/soundfont/src/`.
std::unique_ptr<Sf2Parser> createDefaultSf2Parser();

}  // namespace olysf2sampler::soundfont
