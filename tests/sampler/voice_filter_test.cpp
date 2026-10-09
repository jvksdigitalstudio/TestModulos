#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "olysf2sampler/dsp/filter.hpp"
#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"
#include "olysf2sampler/sampler/voice.hpp"

using namespace olysf2sampler::sampler;
using olysf2sampler::dsp::FilterType;

namespace {

constexpr float kSampleRate = 48000.0f;

// Genera una mezcla de una fundamental grave (200Hz) y un armónico
// agudo (10kHz) — el filtro LowPass debe dejar pasar la primera y
// atenuar fuertemente la segunda.
std::vector<std::int16_t> makeTwoToneWaveform(std::size_t frameCount) {
    std::vector<std::int16_t> data(frameCount);
    for (std::size_t i = 0; i < frameCount; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
        double low = std::sin(2.0 * 3.14159265358979 * 200.0 * t);
        double high = std::sin(2.0 * 3.14159265358979 * 10000.0 * t);
        data[i] = static_cast<std::int16_t>(8000.0 * (0.5 * low + 0.5 * high));
    }
    return data;
}

// Mide "asperezaa" de la señal como proxy barato de contenido de alta
// frecuencia: suma de |diferencias entre muestras consecutivas|. Una
// señal con mucho contenido agudo cambia bruscamente muestra a
// muestra; una filtrada en LowPass cambia más suavemente.
double roughness(const std::vector<float>& buffer) {
    double sum = 0.0;
    for (std::size_t i = 1; i < buffer.size(); ++i) {
        sum += std::abs(static_cast<double>(buffer[i]) - static_cast<double>(buffer[i - 1]));
    }
    return sum;
}

void testVoiceFilterAttenuatesHighFrequencyContent() {
    std::vector<std::int16_t> pcm = makeTwoToneWaveform(8000);

    auto voice = createVoice(kSampleRate);

    SampleSource source;
    source.pcmData = pcm.data();
    source.frameCount = pcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(kSampleRate);
    source.rootNote = 60;
    source.loopEnabled = true;
    source.loopStartFrame = 0;
    source.loopEndFrame = static_cast<std::uint32_t>(pcm.size());

    NoteEvent event;
    event.midiNote = 60;
    event.velocity = 100;
    event.sampleId = 1;

    // --- Sin filtro ---
    voice->trigger(event, source);
    std::vector<float> unfiltered(4000, 0.0f);
    voice->renderBlock(unfiltered.data(), unfiltered.size());
    double roughnessBefore = roughness(unfiltered);

    // --- Con filtro LowPass agresivo (corte muy por debajo de 10kHz) ---
    auto voice2 = createVoice(kSampleRate);
    voice2->setFilterCutoff(FilterType::LowPass, 500.0f, 0.707f);
    voice2->trigger(event, source);
    std::vector<float> filtered(4000, 0.0f);
    voice2->renderBlock(filtered.data(), filtered.size());
    double roughnessAfter = roughness(filtered);

    assert(roughnessAfter < roughnessBefore * 0.5);  // atenuación clara y medible
    std::printf(
        "[tests/sampler] Voice::setFilterCutoff atenúa agudos de verdad (rugosidad %.1f -> "
        "%.1f) OK\n",
        roughnessBefore, roughnessAfter);
}

}  // namespace

int main() {
    testVoiceFilterAttenuatesHighFrequencyContent();
    std::printf("[tests/sampler] filtro por voz: test pasó\n");
    return 0;
}
