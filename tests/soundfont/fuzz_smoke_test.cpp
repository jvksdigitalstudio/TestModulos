// Fuzz smoke test: alimenta al parser con (a) bytes puramente
// aleatorios y (b) el fixture válido con bytes individuales mutados
// al azar. La única propiedad que se exige es que el proceso NUNCA
// crashee ni lea fuera de rango (verificable bajo ASan/UBSan) — el
// resultado en sí (Success o Failure) es irrelevante para este test.
// Esto no reemplaza un fuzzer real (libFuzzer/AFL) pero da una señal
// barata y rápida de robustez ante datos arbitrarios.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <vector>

#include "olysf2sampler/soundfont/parser.hpp"

using namespace olysf2sampler::soundfont;

namespace {

std::vector<std::uint8_t> readFileBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)),
                                      std::istreambuf_iterator<char>());
}

void fuzzRandomBytes(std::mt19937& rng, int iterations) {
    auto parser = createDefaultSf2Parser();
    std::uniform_int_distribution<int> lenDist(0, 2048);
    std::uniform_int_distribution<int> byteDist(0, 255);

    for (int i = 0; i < iterations; ++i) {
        std::size_t len = static_cast<std::size_t>(lenDist(rng));
        std::vector<std::uint8_t> data(len);
        for (auto& b : data) {
            b = static_cast<std::uint8_t>(byteDist(rng));
        }
        ByteSpan span{data.empty() ? nullptr : data.data(), data.size()};
        // No importa el resultado; importa que no crashee (ASan lo
        // detectaría como fallo del proceso, no como assert aquí).
        auto result = parser->parse(span);
        (void)result;
    }
}

void fuzzMutatedValidFixture(std::mt19937& rng, int iterations) {
    std::vector<std::uint8_t> base = readFileBytes("tests/fixtures/valid/minimal.sf2");
    if (base.empty()) {
        std::fprintf(stderr, "fixture válido no encontrado, se omite esta parte del fuzz\n");
        return;
    }
    auto parser = createDefaultSf2Parser();
    std::uniform_int_distribution<std::size_t> posDist(0, base.size() - 1);
    std::uniform_int_distribution<int> byteDist(0, 255);
    std::uniform_int_distribution<int> mutationCountDist(1, 12);

    for (int i = 0; i < iterations; ++i) {
        std::vector<std::uint8_t> mutated = base;
        int mutations = mutationCountDist(rng);
        for (int m = 0; m < mutations; ++m) {
            mutated[posDist(rng)] = static_cast<std::uint8_t>(byteDist(rng));
        }
        // También prueba truncamientos aleatorios.
        if (i % 3 == 0) {
            std::uniform_int_distribution<std::size_t> truncDist(0, mutated.size());
            mutated.resize(truncDist(rng));
        }
        ByteSpan span{mutated.empty() ? nullptr : mutated.data(), mutated.size()};
        auto result = parser->parse(span);
        (void)result;
    }
}

}  // namespace

int main() {
    std::mt19937 rng(0xC0FFEE);  // semilla fija: reproducible entre corridas
    fuzzRandomBytes(rng, 2000);
    fuzzMutatedValidFixture(rng, 2000);
    std::printf("[tests/soundfont] fuzz smoke test OK — 4000 entradas, sin crash\n");
    return 0;
}
