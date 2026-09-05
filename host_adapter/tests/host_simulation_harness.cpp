// Simula, con la mayor fidelidad posible sin tener el Host completo
// aquí, exactamente lo que host/ModuleLoader.cpp hace: dlopen() de la
// .so real, dlsym() de "olyze_module_entry", y recorrido completo del
// ciclo de vida documentado en AudioModuleContract.h — create ->
// initialize -> prepare -> process (con eventos de nota reales) ->
// reset -> shutdown -> destroy -> dlclose.
//
// Esto es lo más cerca que se puede estar de probar la integración real
// sin compilar el Host completo (que es otro proyecto/repo). Si esto
// pasa, el adaptador es funcionalmente correcto contra el contrato
// exacto que el Host espera.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vector>

#include "AudioModuleContract.h"

namespace {

void requireOk(OlyzeResult result, const char* what) {
    if (result != OLYZE_OK) {
        std::fprintf(stderr, "FALLÓ: %s devolvió código %d (esperado OLYZE_OK)\n", what, result);
        std::abort();
    }
}

void diagnosticCallback(void* /*userData*/, OlyzeLogLevel level, const char* component,
                        const char* message) {
    const char* levelStr = "?";
    switch (level) {
        case OLYZE_LOG_DEBUG: levelStr = "DEBUG"; break;
        case OLYZE_LOG_INFO: levelStr = "INFO"; break;
        case OLYZE_LOG_WARNING: levelStr = "WARN"; break;
        case OLYZE_LOG_ERROR: levelStr = "ERROR"; break;
    }
    std::printf("[%s] %s: %s\n", levelStr, component, message);
}

double signalEnergy(const std::vector<float>& buffer) {
    double sum = 0.0;
    for (float s : buffer) sum += static_cast<double>(s) * s;
    return sum;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "uso: %s <ruta a libolysf2sampler_module.so>\n", argv[0]);
        return 1;
    }
    const char* soPath = argv[1];

    // --- Paso 1: dlopen(), igual que ModuleLoader::load() ---
    void* handle = dlopen(soPath, RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        std::fprintf(stderr, "dlopen falló: %s\n", dlerror());
        return 1;
    }
    std::printf("[harness] dlopen(%s) OK\n", soPath);

    // --- Paso 2: dlsym del símbolo único requerido por el contrato ---
    dlerror();  // limpia errores previos
    auto entryFn =
        reinterpret_cast<OlyzeModuleEntryFn>(dlsym(handle, OLYZE_MODULE_ENTRY_SYMBOL));
    const char* dlsymError = dlerror();
    if (dlsymError != nullptr || entryFn == nullptr) {
        std::fprintf(stderr, "dlsym(%s) falló: %s\n", OLYZE_MODULE_ENTRY_SYMBOL,
                    dlsymError ? dlsymError : "puntero nulo");
        dlclose(handle);
        return 1;
    }
    std::printf("[harness] dlsym(%s) OK\n", OLYZE_MODULE_ENTRY_SYMBOL);

    // --- Paso 3: obtener el descriptor, validar versión de ABI ---
    OlyzeModuleDescriptor* descriptor = entryFn();
    assert(descriptor != nullptr);
    // Fix: antes solo se comprobaba abiVersionMajor contra la MISMA
    // macro del header vendorizado con el que se compiló el módulo —
    // una comprobación tautológica que nunca podía fallar, ni siquiera
    // si structSize estuviera mal seteado o el header vendorizado
    // hubiera quedado desincronizado del Host real. Esto es
    // exactamente lo que dejó pasar sin detectar la desincronización
    // real con OlyzeAudioModuleHost (ABI major 1 vendorizado vs 2 real
    // del Host, structSize nunca seteado) — ver
    // host_adapter/contracts/README.md. Ahora replica el chequeo REAL
    // que hace ModuleLoader::load() del Host (ModuleLoader.cpp):
    // structSize primero, porque es el único campo seguro de leer
    // pase lo que pase (es el primero del struct en cualquier versión
    // del ABI), y solo después el resto de los campos.
    assert(descriptor->structSize >= sizeof(OlyzeModuleDescriptor));
    assert(descriptor->abiVersionMajor == OLYZE_MODULE_ABI_VERSION_MAJOR);
    assert(descriptor->category == OLYZE_CATEGORY_SAMPLER);
    std::printf("[harness] structSize=%zu (>= %zu esperado) OK\n", descriptor->structSize,
               sizeof(OlyzeModuleDescriptor));
    std::printf("[harness] módulo: %s v%s (categoría=SAMPLER, ABI %d.%d) OK\n",
               descriptor->moduleId, descriptor->version, descriptor->abiVersionMajor,
               descriptor->abiVersionMinor);

    const OlyzeModuleVTable& vt = descriptor->vtable;
    assert(vt.create && vt.destroy && vt.initialize && vt.prepare && vt.reset &&
          vt.shutdown && vt.process && vt.getParameterCount && vt.getParameterInfo &&
          vt.setParameter && vt.getParameter && vt.loadPreset && vt.savePreset &&
          vt.getMetadataJson && vt.getUiDescriptorJson);
    std::printf("[harness] las 15 funciones del vtable están presentes OK\n");

    // --- Paso 4: create() + initialize() ---
    OlyzeModuleHandle instance = vt.create();
    assert(instance != nullptr);
    requireOk(vt.initialize(instance, diagnosticCallback, nullptr), "initialize()");

    // --- Paso 5: metadata / UI JSON válidos ---
    const char* metadata = vt.getMetadataJson(instance);
    assert(metadata != nullptr && std::strstr(metadata, "olysf2sampler") != nullptr);
    const char* uiDescriptor = vt.getUiDescriptorJson(instance);
    assert(uiDescriptor != nullptr && std::strstr(uiDescriptor, "controls") != nullptr);
    std::printf("[harness] metadata/UI JSON presentes y con forma válida OK\n");

    // --- Paso 6: prepare() con una config realista ---
    OlyzeAudioConfig config{};
    config.sampleRate = 48000;
    config.framesPerBlock = 256;
    config.numInputChannels = 0;  // instrumento: sin entrada de audio
    config.numOutputChannels = 2;  // estéreo, como pediría el Host real
    requireOk(vt.prepare(instance, &config), "prepare()");
    std::printf("[harness] prepare() a 48kHz/256 frames/2 canales OK\n");

    // --- Paso 7: process() SIN eventos -> debe ser silencio (nada disparado) ---
    std::vector<float> leftSilence(256, 0.0f), rightSilence(256, 0.0f);
    float* outSilence[2] = {leftSilence.data(), rightSilence.data()};
    requireOk(vt.process(instance, nullptr, outSilence, 256, nullptr, 0), "process() sin eventos");
    assert(signalEnergy(leftSilence) < 1e-6);
    std::printf("[harness] process() sin eventos produce silencio real OK\n");

    // --- Paso 8: process() CON un NOTE_ON -> debe producir audio real ---
    OlyzeModuleEvent noteOnEvent{};
    noteOnEvent.type = OLYZE_EVENT_NOTE_ON;
    noteOnEvent.frameOffset = 0;
    noteOnEvent.note = 69;  // A4, coincide con el tono de prueba del adaptador
    noteOnEvent.value = 0.8f;  // velocity 0..1

    std::vector<float> leftSound(256, 0.0f), rightSound(256, 0.0f);
    float* outSound[2] = {leftSound.data(), rightSound.data()};
    requireOk(vt.process(instance, nullptr, outSound, 256, &noteOnEvent, 1),
             "process() con NOTE_ON");
    assert(signalEnergy(leftSound) > 1e-3);
    // Verifica que el canal derecho recibió una copia real (duplicado
    // mono->estéreo), no basura sin inicializar.
    assert(std::memcmp(leftSound.data(), rightSound.data(), 256 * sizeof(float)) == 0);
    std::printf("[harness] process() con NOTE_ON produce audio real en ambos canales OK\n");

    // --- Paso 9: NOTE_OFF -> el audio debe decaer con los bloques siguientes ---
    OlyzeModuleEvent noteOffEvent{};
    noteOffEvent.type = OLYZE_EVENT_NOTE_OFF;
    noteOffEvent.note = 69;
    vt.process(instance, nullptr, outSound, 256, &noteOffEvent, 1);

    std::vector<float> leftTail(256, 0.0f), rightTail(256, 0.0f);
    float* outTail[2] = {leftTail.data(), rightTail.data()};
    // Suficientes bloques para completar el release (~0.15s @ 48kHz / 256 = ~29 bloques).
    for (int i = 0; i < 60; ++i) {
        std::fill(leftTail.begin(), leftTail.end(), 0.0f);
        std::fill(rightTail.begin(), rightTail.end(), 0.0f);
        vt.process(instance, nullptr, outTail, 256, nullptr, 0);
    }
    assert(signalEnergy(leftTail) < 1e-6);
    std::printf("[harness] NOTE_OFF decae a silencio real tras el release OK\n");

    // --- Paso 10: parámetros — ahora 3 reales (Volume/Cutoff/Resonance) ---
    assert(vt.getParameterCount(instance) == 3);

    OlyzeParameterInfo volInfo{};
    requireOk(vt.getParameterInfo(instance, 0, &volInfo), "getParameterInfo(0)");
    assert(std::strcmp(volInfo.name, "Master Volume") == 0);
    assert(volInfo.defaultValue == 1.0f);
    std::printf("[harness] getParameterInfo(0) = '%s' [%.1f..%.1f], default %.1f OK\n",
               volInfo.name, volInfo.minValue, volInfo.maxValue, volInfo.defaultValue);

    OlyzeParameterInfo cutoffInfo{};
    requireOk(vt.getParameterInfo(instance, 1, &cutoffInfo), "getParameterInfo(1)");
    assert(std::strcmp(cutoffInfo.name, "Filter Cutoff") == 0);
    assert(std::strcmp(cutoffInfo.unit, "Hz") == 0);

    // Un índice fuera de rango debe fallar limpio, no leer basura.
    OlyzeParameterInfo outOfRange{};
    assert(vt.getParameterInfo(instance, 99, &outOfRange) == OLYZE_ERR_INVALID_ARGUMENT);

    // --- Paso 10b: mover el slider de volumen de verdad reduce el audio medido ---
    OlyzeModuleEvent noteOnForParams{};
    noteOnForParams.type = OLYZE_EVENT_NOTE_ON;
    noteOnForParams.note = 69;
    noteOnForParams.value = 0.8f;
    vt.process(instance, nullptr, outSound, 256, &noteOnForParams, 1);

    std::vector<float> loudLeft(512, 0.0f), loudRight(512, 0.0f);
    float* outLoud[2] = {loudLeft.data(), loudRight.data()};
    vt.process(instance, nullptr, outLoud, 512, nullptr, 0);
    double peakLoud = 0.0;
    for (float s : loudLeft) peakLoud = std::max(peakLoud, static_cast<double>(std::abs(s)));

    OlyzeParameterChange volChange{};
    volChange.parameterId = 0;
    volChange.targetValue = 0.1f;
    volChange.rampMs = 0.0f;
    requireOk(vt.setParameter(instance, volChange), "setParameter(volume=0.1)");
    assert(vt.getParameter(instance, 0) == 0.1f);

    std::vector<float> quietLeft(512, 0.0f), quietRight(512, 0.0f);
    float* outQuiet[2] = {quietLeft.data(), quietRight.data()};
    vt.process(instance, nullptr, outQuiet, 512, nullptr, 0);
    double peakQuiet = 0.0;
    for (float s : quietLeft) peakQuiet = std::max(peakQuiet, static_cast<double>(std::abs(s)));

    assert(peakQuiet < peakLoud * 0.5);
    std::printf(
        "[harness] setParameter(Master Volume, 0.1) via ABI reduce el audio real medido "
        "(%.4f -> %.4f) OK\n",
        peakLoud, peakQuiet);

    // --- Paso 10c: parámetro inválido rechazado limpio ---
    OlyzeParameterChange badChange{};
    badChange.parameterId = 999;
    badChange.targetValue = 1.0f;
    assert(vt.setParameter(instance, badChange) == OLYZE_ERR_INVALID_ARGUMENT);
    std::printf("[harness] setParameter con id inválido rechazado limpio OK\n");

    vt.process(instance, nullptr, outSound, 256, nullptr, 0);  // limpia estado antes de reset

    // --- Paso 11: reset() no debe crashear y debe seguir procesando después ---
    requireOk(vt.reset(instance), "reset()");
    std::vector<float> leftAfterReset(256, 0.0f), rightAfterReset(256, 0.0f);
    float* outAfterReset[2] = {leftAfterReset.data(), rightAfterReset.data()};
    requireOk(vt.process(instance, nullptr, outAfterReset, 256, &noteOnEvent, 1),
             "process() tras reset()");
    assert(signalEnergy(leftAfterReset) > 1e-3);
    std::printf("[harness] reset() seguido de process() sigue produciendo audio OK\n");

    // --- Paso 12: shutdown() + destroy() + dlclose() ---
    vt.shutdown(instance);
    vt.destroy(instance);
    dlclose(handle);
    std::printf("[harness] shutdown()/destroy()/dlclose() sin crash OK\n");

    std::printf("\n[harness] TODOS los pasos del ciclo de vida del Host pasaron\n");
    return 0;
}
