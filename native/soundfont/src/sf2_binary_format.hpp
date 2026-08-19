#pragma once
// native/soundfont/src/sf2_binary_format.hpp — detalle de implementación
// INTERNO, no forma parte de la API pública del módulo (por eso vive en
// src/, no en include/). Define el layout binario crudo de un .sf2
// (tamaños de registro RIFF/phdr/pbag/.../shdr) y un lector con bounds
// checking obligatorio en cada acceso, siguiendo
// docs/architecture/SECURITY.md: todo archivo SF2 es dato no confiable.

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

#include "olysf2sampler/soundfont/parser.hpp"  // ByteSpan

namespace olysf2sampler::soundfont::detail {

// Tamaños de registro fijo según la especificación SoundFont 2.04.
constexpr std::size_t kPhdrRecordSize = 38;
constexpr std::size_t kPbagRecordSize = 4;
constexpr std::size_t kPmodRecordSize = 10;
constexpr std::size_t kPgenRecordSize = 4;
constexpr std::size_t kInstRecordSize = 22;
constexpr std::size_t kIbagRecordSize = 4;
constexpr std::size_t kImodRecordSize = 10;
constexpr std::size_t kIgenRecordSize = 4;
constexpr std::size_t kShdrRecordSize = 46;

/// Lector de bytes con cursor y bounds checking en cada operación.
/// Ninguna función de esta clase puede leer fuera de `span_`; todas
/// devuelven `std::nullopt`/`false` ante un intento de leer más allá
/// del final del buffer, en vez de comportamiento indefinido.
class SafeByteReader {
public:
    explicit SafeByteReader(ByteSpan span) : span_(span) {}

    [[nodiscard]] std::size_t position() const noexcept { return pos_; }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return pos_ <= span_.size ? span_.size - pos_ : 0;
    }
    [[nodiscard]] bool atEnd() const noexcept { return pos_ >= span_.size; }

    /// Reposiciona el cursor de forma absoluta. Devuelve false (y no
    /// mueve el cursor) si `newPos` excede el tamaño del buffer.
    [[nodiscard]] bool seek(std::size_t newPos) noexcept {
        if (newPos > span_.size) {
            return false;
        }
        pos_ = newPos;
        return true;
    }

    [[nodiscard]] std::optional<std::uint8_t> readU8() noexcept {
        if (remaining() < 1) {
            return std::nullopt;
        }
        std::uint8_t v = span_.data[pos_];
        pos_ += 1;
        return v;
    }

    [[nodiscard]] std::optional<std::int8_t> readI8() noexcept {
        auto u = readU8();
        if (!u) {
            return std::nullopt;
        }
        return static_cast<std::int8_t>(*u);
    }

    [[nodiscard]] std::optional<std::uint16_t> readU16LE() noexcept {
        if (remaining() < 2) {
            return std::nullopt;
        }
        std::uint16_t v = static_cast<std::uint16_t>(span_.data[pos_]) |
                           (static_cast<std::uint16_t>(span_.data[pos_ + 1]) << 8);
        pos_ += 2;
        return v;
    }

    [[nodiscard]] std::optional<std::int16_t> readI16LE() noexcept {
        auto u = readU16LE();
        if (!u) {
            return std::nullopt;
        }
        return static_cast<std::int16_t>(*u);
    }

    [[nodiscard]] std::optional<std::uint32_t> readU32LE() noexcept {
        if (remaining() < 4) {
            return std::nullopt;
        }
        std::uint32_t v = static_cast<std::uint32_t>(span_.data[pos_]) |
                           (static_cast<std::uint32_t>(span_.data[pos_ + 1]) << 8) |
                           (static_cast<std::uint32_t>(span_.data[pos_ + 2]) << 16) |
                           (static_cast<std::uint32_t>(span_.data[pos_ + 3]) << 24);
        pos_ += 4;
        return v;
    }

    /// Lee exactamente 4 bytes como FourCC (RIFF/LIST/INFO/phdr/etc.),
    /// sin interpretar como número.
    [[nodiscard]] std::optional<std::string> readFourCC() noexcept {
        if (remaining() < 4) {
            return std::nullopt;
        }
        std::string s(reinterpret_cast<const char*>(&span_.data[pos_]), 4);
        pos_ += 4;
        return s;
    }

    /// Lee `count` bytes como string de longitud fija estilo SF2
    /// (nombre con padding de ceros, SIN asumir null-termination —
    /// se trunca en el primer '\0' si existe, o se usa `count`
    /// completo si no hay ninguno, tal como exige la spec).
    [[nodiscard]] std::optional<std::string> readFixedString(std::size_t count) noexcept {
        if (remaining() < count) {
            return std::nullopt;
        }
        const char* start = reinterpret_cast<const char*>(&span_.data[pos_]);
        std::size_t len = 0;
        while (len < count && start[len] != '\0') {
            ++len;
        }
        std::string s(start, len);
        pos_ += count;
        return s;
    }

    /// Devuelve un ByteSpan (vista, sin copiar) de `count` bytes desde
    /// el cursor actual, y avanza el cursor. nullopt si excede el buffer.
    [[nodiscard]] std::optional<ByteSpan> readSpan(std::size_t count) noexcept {
        if (remaining() < count) {
            return std::nullopt;
        }
        ByteSpan out{&span_.data[pos_], count};
        pos_ += count;
        return out;
    }

    /// Salta `count` bytes sin leerlos. false si excede el buffer.
    [[nodiscard]] bool skip(std::size_t count) noexcept {
        if (remaining() < count) {
            return false;
        }
        pos_ += count;
        return true;
    }

private:
    ByteSpan span_;
    std::size_t pos_{0};
};

/// Multiplicación con detección de overflow para validar
/// `recordCount * recordSize` antes de usarlo como tamaño de
/// asignación o límite de bucle (ver docs/architecture/SECURITY.md
/// regla 1/3). Devuelve nullopt si desborda.
[[nodiscard]] inline std::optional<std::size_t> checkedMul(std::size_t a, std::size_t b) noexcept {
    if (a != 0 && b > (static_cast<std::size_t>(-1) / a)) {
        return std::nullopt;
    }
    return a * b;
}

}  // namespace olysf2sampler::soundfont::detail
