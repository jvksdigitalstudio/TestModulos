// ============================================================================
// Olyze / EliNer — AudioModule Contract (C ABI)
// ============================================================================
// This header is the ONLY thing a module and the Host agree on. It is
// intentionally plain C, not C++ classes/vtables, because modules are
// compiled independently (possibly with a different NDK/STL revision than
// the Host) and loaded at runtime via dlopen(). A C ABI is the only boundary
// that survives that independence reliably.
//
// A module ships as a shared library exposing exactly one symbol:
//
//     OlyzeModuleDescriptor* olyze_module_entry(void);
//
// The Host calls this once after dlopen() to obtain the full function table.
// Everything else — synth internals, effect DSP, voice management — is
// opaque to the Host. The Host never allocates, frees, or interprets a
// module's internal state; it only holds the opaque `handle` returned by
// create().
// ============================================================================

#pragma once

// stdint.h/stddef.h (not <cstdint>/<cstddef>) — this header must remain
// usable from a plain C translation unit, not just C++. The cstdint/
// cstddef forms are C++-only headers (they only guarantee the C names
// land in the global namespace as an implementation detail, not as a
// portability guarantee); the .h forms are the actual C standard headers.
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Versioning — the Host refuses to load a module whose ABI major version
// does not match. This is checked BEFORE any function pointer is called.
//
// ABI major 2 (bumped from 1): OlyzeModuleDescriptor gained a leading
// structSize field (see below), which shifts every other field's memory
// offset. That is a genuine binary-layout break, not a compatible
// addition — any module compiled against ABI major 1 will correctly fail
// the Host's version check rather than have its memory misread.
// ---------------------------------------------------------------------------
#define OLYZE_MODULE_ABI_VERSION_MAJOR 2
#define OLYZE_MODULE_ABI_VERSION_MINOR 0

typedef enum OlyzeModuleCategory {
    OLYZE_CATEGORY_UNKNOWN = 0,
    OLYZE_CATEGORY_SYNTH,
    OLYZE_CATEGORY_SAMPLER,
    OLYZE_CATEGORY_EFFECT,
    OLYZE_CATEGORY_REVERB,
    OLYZE_CATEGORY_DELAY,
    OLYZE_CATEGORY_COMPRESSOR,
    OLYZE_CATEGORY_EQ,
    OLYZE_CATEGORY_FILTER,
    OLYZE_CATEGORY_DISTORTION,
    OLYZE_CATEGORY_SATURATION,
    OLYZE_CATEGORY_CHORUS,
    OLYZE_CATEGORY_FLANGER,
    OLYZE_CATEGORY_LIMITER,
} OlyzeModuleCategory;

typedef enum OlyzeResult {
    OLYZE_OK = 0,
    OLYZE_ERR_INVALID_STATE,
    OLYZE_ERR_INVALID_ARGUMENT,
    OLYZE_ERR_OUT_OF_MEMORY,
    OLYZE_ERR_UNSUPPORTED,
    OLYZE_ERR_INTERNAL,
} OlyzeResult;

// Opaque handle to a module instance. Owned and interpreted only by the
// module itself; the Host treats it as a black box.
typedef void* OlyzeModuleHandle;

// ---------------------------------------------------------------------------
// Audio configuration passed to prepare(). A module may report back that it
// requires a different buffer size (e.g. convolution reverbs); the Host
// honors this when possible or fails the load with a clear diagnostic.
// ---------------------------------------------------------------------------
typedef struct OlyzeAudioConfig {
    int32_t sampleRate;
    int32_t framesPerBlock;
    int32_t numInputChannels;   // 0 for instruments with no audio input
    int32_t numOutputChannels;
} OlyzeAudioConfig;

// ---------------------------------------------------------------------------
// Parameters — modules expose a flat list, each with a stable numeric id.
// The Host never needs to know what a parameter *does*; it only needs id,
// range, and display info to render generic controls when the module has
// no custom UI descriptor for a given control.
// ---------------------------------------------------------------------------
typedef struct OlyzeParameterInfo {
    int32_t id;
    const char* name;
    const char* unit;          // e.g. "dB", "Hz", "%", "" — may be empty
    float minValue;
    float maxValue;
    float defaultValue;
    int32_t isDiscrete;        // 0 = continuous, 1 = stepped/enum
} OlyzeParameterInfo;

// Smoothed parameter change request. rampMs == 0 means apply immediately
// on the next processed block (still audio-thread safe: the module is
// responsible for internal smoothing to avoid zipper noise).
typedef struct OlyzeParameterChange {
    int32_t parameterId;
    float targetValue;
    float rampMs;
} OlyzeParameterChange;

// ---------------------------------------------------------------------------
// MIDI-style event, used both for the virtual keyboard and external MIDI.
// Timestamps are frame-offsets within the current block for sample-accurate
// scheduling; the Host guarantees offset < framesPerBlock.
// ---------------------------------------------------------------------------
typedef enum OlyzeEventType {
    OLYZE_EVENT_NOTE_ON = 0,
    OLYZE_EVENT_NOTE_OFF,
    OLYZE_EVENT_PITCH_BEND,
    OLYZE_EVENT_MOD_WHEEL,
    OLYZE_EVENT_SUSTAIN,
    OLYZE_EVENT_CC,
} OlyzeEventType;

typedef struct OlyzeModuleEvent {
    OlyzeEventType type;
    int32_t frameOffset;
    int32_t note;        // for NOTE_ON/OFF: MIDI note number
    float value;         // velocity (0..1), pitch bend (-1..1), CC value (0..1), etc.
    int32_t ccNumber;    // only used when type == OLYZE_EVENT_CC
} OlyzeModuleEvent;

// ---------------------------------------------------------------------------
// Metadata / UI descriptors are returned as UTF-8 JSON strings owned by the
// module (valid until the next call into the module, or until destroy()).
// This keeps the ABI stable even as metadata/UI schemas evolve — the Host
// parses JSON on a non-realtime thread only.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Diagnostic callback — the module reports structured diagnostics through
// this function pointer rather than doing its own logging I/O. The Host
// supplies an implementation backed by the lock-free diagnostics ring
// buffer, so this is safe to call from the audio thread.
// ---------------------------------------------------------------------------
typedef enum OlyzeLogLevel {
    OLYZE_LOG_DEBUG = 0,
    OLYZE_LOG_INFO,
    OLYZE_LOG_WARNING,
    OLYZE_LOG_ERROR,
} OlyzeLogLevel;

typedef void (*OlyzeDiagnosticFn)(
    void* userData,
    OlyzeLogLevel level,
    const char* component,
    const char* message
);

// ---------------------------------------------------------------------------
// The function table every module must fill in and return from
// olyze_module_entry(). All functions except process() may block briefly
// (file I/O, allocation); process() MUST be realtime-safe: no locks that can
// be held by a non-audio thread, no allocation, no I/O.
// ---------------------------------------------------------------------------
typedef struct OlyzeModuleVTable {
    // Lifecycle
    OlyzeModuleHandle (*create)(void);
    void (*destroy)(OlyzeModuleHandle handle);

    OlyzeResult (*initialize)(OlyzeModuleHandle handle,
                               OlyzeDiagnosticFn diagnosticFn,
                               void* diagnosticUserData);

    // prepare() may be called multiple times (e.g. sample rate change).
    // The module may adjust `config->framesPerBlock` to its own requirement;
    // the Host re-reads the struct after this call.
    OlyzeResult (*prepare)(OlyzeModuleHandle handle, OlyzeAudioConfig* config);

    OlyzeResult (*reset)(OlyzeModuleHandle handle);
    void (*shutdown)(OlyzeModuleHandle handle);

    // Realtime audio processing. Non-interleaved float buffers.
    // Returns OLYZE_OK, or an error code WITHOUT throwing/blocking — the
    // Host treats a non-OK return as a module-side xrun and reports it via
    // diagnostics, not via exceptions.
    OlyzeResult (*process)(OlyzeModuleHandle handle,
                            const float* const* inputChannels,
                            float* const* outputChannels,
                            int32_t numFrames,
                            const OlyzeModuleEvent* events,
                            int32_t numEvents);

    // Parameters
    int32_t (*getParameterCount)(OlyzeModuleHandle handle);
    OlyzeResult (*getParameterInfo)(OlyzeModuleHandle handle, int32_t index, OlyzeParameterInfo* outInfo);
    OlyzeResult (*setParameter)(OlyzeModuleHandle handle, OlyzeParameterChange change);
    float (*getParameter)(OlyzeModuleHandle handle, int32_t parameterId);

    // Presets — opaque binary blob; the Host persists bytes without
    // interpreting them. Module owns the buffer returned by savePreset()
    // until the next call into the module.
    OlyzeResult (*loadPreset)(OlyzeModuleHandle handle, const uint8_t* data, size_t size);
    OlyzeResult (*savePreset)(OlyzeModuleHandle handle, const uint8_t** outData, size_t* outSize);

    // Metadata / UI — UTF-8 JSON, module-owned pointers.
    const char* (*getMetadataJson)(OlyzeModuleHandle handle);
    const char* (*getUiDescriptorJson)(OlyzeModuleHandle handle);
} OlyzeModuleVTable;

typedef struct OlyzeModuleDescriptor {
    // MUST be the first field. The module always sets this to
    // sizeof(OlyzeModuleDescriptor) as compiled into the MODULE's own
    // build. The Host checks this is at least as large as its own
    // sizeof(OlyzeModuleDescriptor) (as compiled into the HOST's build)
    // before trusting any field beyond this one — see
    // ModuleLoader::load(). This is what lets the ABI grow (new trailing
    // fields, in a future minor or major version) without the Host
    // silently reading past memory a module built against an older,
    // smaller header actually allocated.
    size_t structSize;
    int32_t abiVersionMajor;
    int32_t abiVersionMinor;
    OlyzeModuleCategory category;
    const char* moduleId;       // stable reverse-DNS style id, module-owned
    const char* displayName;
    const char* version;        // module's own semantic version string
    OlyzeModuleVTable vtable;
} OlyzeModuleDescriptor;

// The single required export.
typedef OlyzeModuleDescriptor* (*OlyzeModuleEntryFn)(void);
#define OLYZE_MODULE_ENTRY_SYMBOL "olyze_module_entry"

#ifdef __cplusplus
}
#endif
