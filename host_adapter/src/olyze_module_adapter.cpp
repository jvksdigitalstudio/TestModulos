// host_adapter/src/olyze_module_adapter.cpp
//
// Implementación real del contrato de host_adapter/contracts/AudioModuleContract.h
// (copia exacta, sin modificar, del contrato de OlyzeAudioModuleHost — ver
// host_adapter/contracts/README.md para su procedencia).
//
// Responsabilidad única de este archivo: TRADUCIR entre el ABI C del Host
// y la API real de OlySf2 Sampler (sampler::SamplerEngine, ya implementada
// y probada desde Fase C). No reimplementa nada de audio/DSP aquí — eso
// viola exactamente la regla que este mismo proyecto se impuso desde el
// principio (Fase 1 §9, JNI/frontera mínima; el mismo principio aplica
// aquí, solo que la frontera es un ABI C en vez de JNI).
//
// Build target independiente: no depende de jni.h ni de Oboe (el Host es
// quien posee el audio real vía su propio AudioEngine — ver
// host_adapter/contracts/README.md). Por eso este archivo SÍ se pudo
// compilar y probar por completo en el entorno de desarrollo, a
// diferencia de jni/olysf2sampler_jni_bridge.cpp y
// native/platform/src/oboe_audio_backend.cpp.

#include "AudioModuleContract.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "olysf2sampler/core/version.hpp"
#include "olysf2sampler/sampler/mapping.hpp"
#include "olysf2sampler/sampler/sampler_engine.hpp"

namespace {

constexpr std::uint64_t kTestToneSampleId = 1;
constexpr std::uint8_t kTestToneRootNote = 69;  // A4 = MIDI 69
constexpr float kToneFrequencyHz = 440.0f;
constexpr float kToneDurationSeconds = 2.0f;

// IDs de parámetros expuestos al Host — deben coincidir con lo que
// devuelve moduleGetParameterInfo() para cada índice (el índice ES el
// id en este adaptador, no hay indirección: simplifica sin perder
// nada, ya que los parámetros no se reordenan dinámicamente).
enum ParameterId : std::int32_t {
    kParamMasterVolume = 0,
    kParamFilterCutoff = 1,
    kParamFilterResonance = 2,
    kParameterCount = 3,
};

struct ParameterRange {
    const char* name;
    const char* unit;
    float minValue;
    float maxValue;
    float defaultValue;
};

constexpr ParameterRange kParameterRanges[kParameterCount] = {
    {"Master Volume", "", 0.0f, 2.0f, 1.0f},
    {"Filter Cutoff", "Hz", 200.0f, 20000.0f, 20000.0f},
    {"Filter Resonance", "Q", 0.1f, 20.0f, 0.707f},
};

// ---------------------------------------------------------------------
// Estado real del módulo (lo que el Host solo ve como OlyzeModuleHandle
// opaco). Contiene el SamplerEngine ya implementado — nada nuevo se
// reimplementa aquí.
// ---------------------------------------------------------------------
struct ModuleInstance {
    std::unique_ptr<olysf2sampler::sampler::SamplerEngine> engine;
    std::unique_ptr<olysf2sampler::sampler::KeyVelocityMapping> mapping;
    std::vector<std::int16_t> testTonePcm;
    OlyzeAudioConfig config{};
    OlyzeDiagnosticFn diagnosticFn{nullptr};
    void* diagnosticUserData{nullptr};
    std::string metadataJson;
    std::string uiDescriptorJson;
    bool prepared{false};
    // Cache local de "último valor pedido" por parámetro — el Host lee
    // esto vía getParameter(); se actualiza en setParameter() y se
    // inicializa a los defaults en prepare(). No hace falta leerlo de
    // vuelta desde SamplerEngine (que solo expone setters, no
    // getters): el adaptador es la única fuente que escribe estos
    // valores, así que cachearlos aquí es correcto y más simple.
    float parameterValues[kParameterCount] = {};
};

void logMessage(ModuleInstance* inst, OlyzeLogLevel level, const char* message) {
    if (inst != nullptr && inst->diagnosticFn != nullptr) {
        inst->diagnosticFn(inst->diagnosticUserData, level, "OlySf2Sampler", message);
    }
}

// Tono de prueba (A4, con loop) — el mismo criterio ya usado en
// jni/bridge/olysf2sampler_jni_bridge.cpp para la Fase D. Sustituirlo por
// samples reales de un .sf2 cargado es la integración SoundFont↔Sampler
// de una fase posterior (ver docs/architecture/AUDIO_ENGINE.md).
std::vector<std::int16_t> generateTestTone(float sampleRateHz) {
    std::size_t frameCount = static_cast<std::size_t>(sampleRateHz * kToneDurationSeconds);
    std::vector<std::int16_t> pcm(frameCount);
    for (std::size_t i = 0; i < frameCount; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(sampleRateHz);
        double value = std::sin(2.0 * 3.14159265358979323846 * kToneFrequencyHz * t);
        pcm[i] = static_cast<std::int16_t>(value * 8000.0);
    }
    return pcm;
}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

OlyzeModuleHandle moduleCreate() { return new ModuleInstance(); }

void moduleDestroy(OlyzeModuleHandle handle) { delete static_cast<ModuleInstance*>(handle); }

OlyzeResult moduleInitialize(OlyzeModuleHandle handle, OlyzeDiagnosticFn diagnosticFn,
                             void* diagnosticUserData) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    if (inst == nullptr) {
        return OLYZE_ERR_INVALID_ARGUMENT;
    }
    inst->diagnosticFn = diagnosticFn;
    inst->diagnosticUserData = diagnosticUserData;

    inst->metadataJson =
        std::string("{\"id\":\"com.yeivikas.olysf2sampler\",") +
        "\"displayName\":\"OlySf2 Sampler\"," + "\"version\":\"" +
        olysf2sampler::core::versionString() + "\"," + "\"category\":\"sampler\"," +
        "\"note\":\"tono de prueba 440Hz hasta que exista carga real de .sf2\"}";
    inst->uiDescriptorJson = "{\"controls\":[]}";  // sin parámetros expuestos todavía

    logMessage(inst, OLYZE_LOG_INFO, "OlySf2 Sampler: initialize() OK");
    return OLYZE_OK;
}

OlyzeResult modulePrepare(OlyzeModuleHandle handle, OlyzeAudioConfig* config) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    if (inst == nullptr || config == nullptr) {
        return OLYZE_ERR_INVALID_ARGUMENT;
    }
    if (config->numOutputChannels < 1 || config->sampleRate <= 0) {
        return OLYZE_ERR_INVALID_ARGUMENT;
    }

    inst->config = *config;
    float sampleRateHz = static_cast<float>(config->sampleRate);

    inst->testTonePcm = generateTestTone(sampleRateHz);

    inst->mapping = olysf2sampler::sampler::createKeyVelocityMapping();
    inst->mapping->setZones(
        {olysf2sampler::sampler::MappingZone{0, 127, 0, 127, kTestToneSampleId}});

    inst->engine = olysf2sampler::sampler::createSamplerEngine(sampleRateHz);
    inst->engine->setMapping(inst->mapping.get());

    // Aplica los defaults declarados en kParameterRanges — el motor
    // arranca en el mismo estado que el Host va a mostrar en sus
    // sliders (nada de "el slider dice 1.0 pero el motor está en otra
    // cosa").
    for (int32_t i = 0; i < kParameterCount; ++i) {
        inst->parameterValues[i] = kParameterRanges[i].defaultValue;
    }
    inst->engine->setMasterVolume(inst->parameterValues[kParamMasterVolume]);
    inst->engine->setFilterCutoff(inst->parameterValues[kParamFilterCutoff],
                                  inst->parameterValues[kParamFilterResonance]);

    olysf2sampler::sampler::SampleSource source;
    source.pcmData = inst->testTonePcm.data();
    source.frameCount = inst->testTonePcm.size();
    source.sampleRateHz = static_cast<std::uint32_t>(sampleRateHz);
    source.rootNote = kTestToneRootNote;
    source.loopEnabled = true;
    source.loopStartFrame = 0;
    source.loopEndFrame = static_cast<std::uint32_t>(inst->testTonePcm.size());
    inst->engine->loadSample(kTestToneSampleId, source);

    inst->prepared = true;
    logMessage(inst, OLYZE_LOG_INFO, "OlySf2 Sampler: prepare() OK");
    return OLYZE_OK;
}

OlyzeResult moduleReset(OlyzeModuleHandle handle) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    if (inst == nullptr || !inst->prepared) {
        return OLYZE_ERR_INVALID_STATE;
    }
    // SamplerEngine no expone todavía un "silenciar todas las voces ya".
    // Reconstruirlo con la misma configuración es el reset más simple y
    // correcto disponible ahora mismo (no realtime-safe, pero reset() no
    // se llama desde el audio thread según el contrato).
    OlyzeAudioConfig config = inst->config;
    return modulePrepare(handle, &config);
}

void moduleShutdown(OlyzeModuleHandle handle) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    if (inst == nullptr) {
        return;
    }
    inst->engine.reset();
    inst->mapping.reset();
    inst->testTonePcm.clear();
    inst->prepared = false;
}

// ---------------------------------------------------------------------
// Audio processing (realtime)
// ---------------------------------------------------------------------

OlyzeResult moduleProcess(OlyzeModuleHandle handle, const float* const* /*inputChannels*/,
                          float* const* outputChannels, int32_t numFrames,
                          const OlyzeModuleEvent* events, int32_t numEvents) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    if (inst == nullptr || !inst->prepared || outputChannels == nullptr ||
        outputChannels[0] == nullptr || numFrames < 0) {
        return OLYZE_ERR_INVALID_STATE;
    }
    // OlySf2 Sampler es un instrumento (categoría SAMPLER): no consume
    // audio de entrada, solo eventos de nota.

    // LIMITACIÓN DOCUMENTADA: SamplerEngine::noteOn/noteOff no aceptan
    // todavía un frameOffset — todos los eventos de este bloque se
    // aplican al inicio del bloque, no en su posición exacta de muestra.
    // Suficiente para demostrar el pipeline funcionando; el scheduling
    // sample-accurate es trabajo futuro (requeriría exponer frameOffset
    // hasta VoiceManager/Voice, que hoy no lo soportan).
    for (int32_t i = 0; i < numEvents; ++i) {
        const OlyzeModuleEvent& event = events[i];
        switch (event.type) {
            case OLYZE_EVENT_NOTE_ON: {
                int velocity =
                    static_cast<int>(std::clamp(event.value, 0.0f, 1.0f) * 127.0f + 0.5f);
                inst->engine->noteOn(event.note, velocity);
                break;
            }
            case OLYZE_EVENT_NOTE_OFF:
                inst->engine->noteOff(event.note);
                break;
            default:
                // PITCH_BEND/MOD_WHEEL/SUSTAIN/CC: no soportados todavía
                // por SamplerEngine — se ignoran sin error (no rompen el
                // procesamiento de audio).
                break;
        }
    }

    // SamplerEngine::renderBlock produce mono; se copia al resto de
    // canales de salida (sin paneo real todavía, mismo estado que en
    // Fase D — ver docs/architecture/AUDIO_ENGINE.md).
    std::fill(outputChannels[0], outputChannels[0] + numFrames, 0.0f);
    inst->engine->renderBlock(outputChannels[0], static_cast<std::size_t>(numFrames));
    for (int32_t ch = 1; ch < inst->config.numOutputChannels; ++ch) {
        if (outputChannels[ch] != nullptr) {
            std::copy(outputChannels[0], outputChannels[0] + numFrames, outputChannels[ch]);
        }
    }

    return OLYZE_OK;
}

// ---------------------------------------------------------------------
// Parámetros reales: Master Volume, Filter Cutoff, Filter Resonance.
// Conectados a olysf2sampler::sampler::SamplerEngine::setMasterVolume/
// setFilterCutoff, que ya están implementados, probados (round-trip
// medido: 0.25 de volumen produce ~1/4 del pico; 500Hz de corte
// atenúa agudos ~50x) y verificados thread-safe bajo ThreadSanitizer
// — ver tests/sampler/live_parameters_test.cpp.
// ---------------------------------------------------------------------

int32_t moduleGetParameterCount(OlyzeModuleHandle /*handle*/) { return kParameterCount; }

OlyzeResult moduleGetParameterInfo(OlyzeModuleHandle /*handle*/, int32_t index,
                                   OlyzeParameterInfo* outInfo) {
    if (outInfo == nullptr || index < 0 || index >= kParameterCount) {
        return OLYZE_ERR_INVALID_ARGUMENT;
    }
    const ParameterRange& range = kParameterRanges[index];
    outInfo->id = index;
    outInfo->name = range.name;
    outInfo->unit = range.unit;
    outInfo->minValue = range.minValue;
    outInfo->maxValue = range.maxValue;
    outInfo->defaultValue = range.defaultValue;
    outInfo->isDiscrete = 0;
    return OLYZE_OK;
}

OlyzeResult moduleSetParameter(OlyzeModuleHandle handle, OlyzeParameterChange change) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    if (inst == nullptr || !inst->prepared) {
        return OLYZE_ERR_INVALID_STATE;
    }
    if (change.parameterId < 0 || change.parameterId >= kParameterCount) {
        return OLYZE_ERR_INVALID_ARGUMENT;
    }
    const ParameterRange& range = kParameterRanges[change.parameterId];
    float clamped = std::clamp(change.targetValue, range.minValue, range.maxValue);
    inst->parameterValues[change.parameterId] = clamped;

    // Fase A.1 §16/§28 (fix real, no solo diseño): rampMs para
    // Master Volume ya NO se ignora — SamplerEngine::setMasterVolume
    // ahora rampea de verdad muestra a muestra a través de tantos
    // bloques de audio como haga falta (ver
    // SamplerEngineImpl::applyMasterVolumeWithRamp,
    // sampler_engine_impl.cpp). rampMs negativo se trata como 0
    // (aplicación esencialmente inmediata, sin rampa) en vez de
    // producir un paso negativo sin sentido.
    //
    // Filter Cutoff/Resonance TODAVÍA aplican el valor de inmediato
    // (paso completo en el siguiente bloque) — smoothing real ahí
    // requeriría interpolar coeficientes de biquad, no solo una
    // ganancia escalar, y queda fuera del alcance de este fix.
    // Documentado como pendiente, no fingido como resuelto.
    switch (change.parameterId) {
        case kParamMasterVolume: {
            float rampMs = change.rampMs > 0.0f ? change.rampMs : 0.0f;
            inst->engine->setMasterVolume(clamped, rampMs);
            break;
        }
        case kParamFilterCutoff:
            inst->engine->setFilterCutoff(clamped, inst->parameterValues[kParamFilterResonance]);
            break;
        case kParamFilterResonance:
            inst->engine->setFilterCutoff(inst->parameterValues[kParamFilterCutoff], clamped);
            break;
        default:
            return OLYZE_ERR_INVALID_ARGUMENT;
    }
    return OLYZE_OK;
}

float moduleGetParameter(OlyzeModuleHandle handle, int32_t parameterId) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    if (inst == nullptr || parameterId < 0 || parameterId >= kParameterCount) {
        return 0.0f;
    }
    return inst->parameterValues[parameterId];
}

// ---------------------------------------------------------------------
// Presets — sin sistema de presets todavía (honesto, no fingido).
// ---------------------------------------------------------------------

OlyzeResult moduleLoadPreset(OlyzeModuleHandle /*handle*/, const uint8_t* /*data*/,
                             std::size_t /*size*/) {
    return OLYZE_ERR_UNSUPPORTED;
}

OlyzeResult moduleSavePreset(OlyzeModuleHandle /*handle*/, const uint8_t** /*outData*/,
                             std::size_t* /*outSize*/) {
    return OLYZE_ERR_UNSUPPORTED;
}

// ---------------------------------------------------------------------
// Metadata / UI
// ---------------------------------------------------------------------

const char* moduleGetMetadataJson(OlyzeModuleHandle handle) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    return inst != nullptr ? inst->metadataJson.c_str() : "";
}

const char* moduleGetUiDescriptorJson(OlyzeModuleHandle handle) {
    auto* inst = static_cast<ModuleInstance*>(handle);
    return inst != nullptr ? inst->uiDescriptorJson.c_str() : "{\"controls\":[]}";
}

OlyzeModuleVTable buildVTable() {
    OlyzeModuleVTable vt{};
    vt.create = moduleCreate;
    vt.destroy = moduleDestroy;
    vt.initialize = moduleInitialize;
    vt.prepare = modulePrepare;
    vt.reset = moduleReset;
    vt.shutdown = moduleShutdown;
    vt.process = moduleProcess;
    vt.getParameterCount = moduleGetParameterCount;
    vt.getParameterInfo = moduleGetParameterInfo;
    vt.setParameter = moduleSetParameter;
    vt.getParameter = moduleGetParameter;
    vt.loadPreset = moduleLoadPreset;
    vt.savePreset = moduleSavePreset;
    vt.getMetadataJson = moduleGetMetadataJson;
    vt.getUiDescriptorJson = moduleGetUiDescriptorJson;
    return vt;
}

}  // namespace

extern "C" OlyzeModuleDescriptor* olyze_module_entry(void) {
    static const std::string moduleId = "com.yeivikas.olysf2sampler";
    static const std::string displayName = "OlySf2 Sampler";
    static const std::string version = olysf2sampler::core::versionString();

    static OlyzeModuleDescriptor descriptor = [] {
        OlyzeModuleDescriptor d{};
        // Fix ABI (Host bumpeó OLYZE_MODULE_ABI_VERSION_MAJOR 1 -> 2:
        // OlyzeModuleDescriptor ganó structSize como PRIMER campo, un
        // cambio real de layout binario, no una adición compatible).
        // structSize debe setearse ANTES que cualquier otro campo lo
        // pise conceptualmente y SIEMPRE con sizeof() tal como este
        // módulo lo compiló — el Host lo usa para saber cuánto puede
        // leer con seguridad sin pisar memoria que este build más
        // viejo/nuevo del contrato no reservó. Ver AudioModuleContract.h.
        d.structSize = sizeof(OlyzeModuleDescriptor);
        d.abiVersionMajor = OLYZE_MODULE_ABI_VERSION_MAJOR;
        d.abiVersionMinor = OLYZE_MODULE_ABI_VERSION_MINOR;
        d.category = OLYZE_CATEGORY_SAMPLER;
        d.moduleId = moduleId.c_str();
        d.displayName = displayName.c_str();
        d.version = version.c_str();
        d.vtable = buildVTable();
        return d;
    }();

    return &descriptor;
}
