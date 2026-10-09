// Smoke test de integración de OlySf2 Sampler: demuestra que
//   1. los 14 módulos host-buildable del núcleo compilan juntos;
//   2. los contratos principales son referenciables desde un mismo
//      binario sin colisión de símbolos ni dependencias circulares;
//   3. el tipo Result<T> de olysf2sampler_core funciona como se espera.
//
// No es un test unitario exhaustivo: eso llega junto con cada
// implementación concreta en fases posteriores.

#include <cassert>
#include <cmath>
#include <cstdio>

#include "olysf2sampler/core/result.hpp"
#include "olysf2sampler/core/version.hpp"
#include "olysf2sampler/device/device_capabilities.hpp"
#include "olysf2sampler/diagnostics/audio_metrics.hpp"
#include "olysf2sampler/diagnostics/logger.hpp"
#include "olysf2sampler/dsp/envelope.hpp"
#include "olysf2sampler/dsp/filter.hpp"
#include "olysf2sampler/dsp/gain.hpp"
#include "olysf2sampler/dsp/interpolation.hpp"
#include "olysf2sampler/dsp/lfo.hpp"
#include "olysf2sampler/dsp/pan.hpp"
#include "olysf2sampler/dsp/resampler.hpp"
#include "olysf2sampler/memory/buffer_pool.hpp"
#include "olysf2sampler/midi/midi_event.hpp"
#include "olysf2sampler/midi/midi_manager.hpp"
#include "olysf2sampler/platform/audio_backend.hpp"
#include "olysf2sampler/audio/audio_engine.hpp"
#include "olysf2sampler/resources/resource_loader.hpp"
#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/modulator.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"
#include "olysf2sampler/sampler/voice.hpp"
#include "olysf2sampler/sampler/voice_manager.hpp"
#include "olysf2sampler/samples/loop_point.hpp"
#include "olysf2sampler/samples/sample_io.hpp"
#include "olysf2sampler/samples/sample_processor.hpp"
#include "olysf2sampler/scheduler/task_scheduler.hpp"
#include "olysf2sampler/soundfont/model.hpp"
#include "olysf2sampler/soundfont/parser.hpp"
#include "olysf2sampler/soundfont/serialization.hpp"
#include "olysf2sampler/soundfont/validator.hpp"
#include "olysf2sampler/soundfont/writer.hpp"
#include "olysf2sampler/threading/audio_thread.hpp"
#include "olysf2sampler/threading/worker_thread.hpp"

namespace {

void testResultOkAndFail() {
    auto ok = olysf2sampler::core::Result<int>::ok(42);
    assert(ok.isOk());
    assert(ok.value() == 42);

    auto err = olysf2sampler::core::Result<int>::fail(
        olysf2sampler::core::Error{olysf2sampler::core::ErrorCode::NotImplemented, "not yet"});
    assert(err.isError());
    assert(err.error().code == olysf2sampler::core::ErrorCode::NotImplemented);
}

void testVoidResult() {
    auto ok = olysf2sampler::core::VoidResult::ok();
    assert(ok.isOk());
}

void testSoundFontModelDefaults() {
    olysf2sampler::soundfont::SoundFontModel model;
    assert(model.samples.empty());
    assert(model.instruments.empty());
    assert(model.presets.empty());
}

void testMappingZoneAggregation() {
    olysf2sampler::sampler::MappingZone zone;
    zone.lowNote = 60;
    zone.highNote = 72;
    assert(zone.lowNote < zone.highNote);
}

void testVersionStringIsStable() {
    const char* v1 = olysf2sampler::core::versionString();
    const char* v2 = olysf2sampler::core::versionString();
    assert(std::string(v1) == std::string(v2));
}

void testDspGainRoundTrip() {
    const float linear = olysf2sampler::dsp::decibelsToLinear(0.0f);
    assert(std::abs(linear - 1.0f) < 1e-5f);

    float samples[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    olysf2sampler::dsp::applyGain(samples, 4, 0.5f);
    for (float s : samples) {
        assert(std::abs(s - 0.5f) < 1e-5f);
    }
}

void testDspConstantPowerPan() {
    const auto center = olysf2sampler::dsp::computeConstantPowerPan(0.0f);
    // En el centro, ambos canales deben tener la misma ganancia y la
    // suma de sus cuadrados debe ser 1.0 (potencia constante).
    assert(std::abs(center.left - center.right) < 1e-5f);
    const float power = center.left * center.left + center.right * center.right;
    assert(std::abs(power - 1.0f) < 1e-4f);

    const auto hardLeft = olysf2sampler::dsp::computeConstantPowerPan(-1.0f);
    assert(hardLeft.right < 1e-4f);
}

void testDspLinearInterpolation() {
    const float mid = olysf2sampler::dsp::interpolateSample(
        olysf2sampler::dsp::InterpolationMethod::Linear, 0.0f, 0.0f, 10.0f, 10.0f, 0.5f);
    assert(std::abs(mid - 5.0f) < 1e-5f);
}

}  // namespace

int main() {
    testResultOkAndFail();
    testVoidResult();
    testSoundFontModelDefaults();
    testMappingZoneAggregation();
    testVersionStringIsStable();
    testDspGainRoundTrip();
    testDspConstantPowerPan();
    testDspLinearInterpolation();

    std::printf("[tests/integration] smoke test OK — OlySf2 Sampler version %s\n",
                olysf2sampler::core::versionString());
    return 0;
}
