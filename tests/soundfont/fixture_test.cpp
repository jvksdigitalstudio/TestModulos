// Ejercita los fixtures reales de tests/fixtures/ leyéndolos del
// disco (no en memoria), confirmando que:
//   - el SF2 válido se parsea y valida correctamente;
//   - cada fixture malformado es rechazado con MalformedInput,
//     nunca con un crash ni con lectura fuera de rango (ver
//     docs/architecture/SECURITY.md).

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "olysf2sampler/soundfont/parser.hpp"
#include "olysf2sampler/soundfont/validator.hpp"

using namespace olysf2sampler::soundfont;

namespace {

bool readFile(const std::string& path, std::vector<std::uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    std::size_t readBytes = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return readBytes == out.size();
}

void testValidFixtureParsesAndValidates() {
    std::vector<std::uint8_t> bytes;
    bool ok = readFile("tests/fixtures/valid/minimal.sf2", bytes);
    assert(ok);
    assert(!bytes.empty());

    auto parser = createDefaultSf2Parser();
    ByteSpan span{bytes.data(), bytes.size()};
    auto result = parser->parse(span);
    assert(result.isOk());

    auto validator = createDefaultSf2Validator();
    ValidationReport report = validator->validate(result.value());
    assert(report.isValid);

    std::printf("[tests/soundfont] fixture válido: parsea y valida OK (%zu bytes)\n",
                bytes.size());
}

void testMalformedFixtureIsRejected(const std::string& path) {
    std::vector<std::uint8_t> bytes;
    bool ok = readFile(path, bytes);
    assert(ok);

    auto parser = createDefaultSf2Parser();
    ByteSpan span{bytes.data(), bytes.size()};
    auto result = parser->parse(span);
    assert(result.isError());
    assert(result.error().code == olysf2sampler::core::ErrorCode::MalformedInput);

    std::printf("[tests/soundfont] fixture malformado '%s' rechazado correctamente: %s\n",
                path.c_str(), result.error().message.c_str());
}

}  // namespace

int main() {
    testValidFixtureParsesAndValidates();
    testMalformedFixtureIsRejected("tests/fixtures/malformed/not_riff.sf2");
    testMalformedFixtureIsRejected("tests/fixtures/malformed/truncated_pdta.sf2");
    testMalformedFixtureIsRejected("tests/fixtures/malformed/bad_riff_size.sf2");
    std::printf("[tests/soundfont] todos los tests de fixtures pasaron\n");
    return 0;
}
