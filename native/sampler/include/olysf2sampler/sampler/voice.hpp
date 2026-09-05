#pragma once
// native/sampler/voice — una única voz de reproducción (un
// disparo de nota). Responsabilidad única: mantener el estado de una
// voz activa (posición de lectura, envolvente, filtro asociado).
// No decide polifonía (eso es voice_manager) ni mapeo (eso es mapping).

#include <cstddef>
#include <cstdint>
#include <memory>

#include "olysf2sampler/dsp/filter.hpp"

namespace olysf2sampler::sampler {

using VoiceId = std::uint32_t;
constexpr VoiceId kInvalidVoiceId = 0;

enum class VoiceState {
    Idle,
    Playing,
    Releasing,
};

struct NoteEvent {
    int midiNote{0};
    int velocity{0};
    std::uint64_t sampleId{0};

    /// Ajuste fino de afinación en cents (-100..+100 típico, sin límite
    /// artificial impuesto aquí — ver docs/architecture/FILE_AUDIT.md
    /// §31). Compuesto por: pitch correction del sample header SF2 +
    /// tuning del preset/instrumento + pitch bend en vivo. La
    /// resolución de a cuántos semitonos/ratio de resampling se
    /// traduce esto es responsabilidad de Voice::trigger, típicamente
    /// delegando en dsp::Resampler::setRatio.
    float fineTuneCents{0.0f};
};

/// Interpolation method a usar por esta voz al leer el sample entre
/// posiciones fraccionales (ver dsp::InterpolationMethod). Se separa
/// del NoteEvent porque es una preferencia de calidad/rendimiento del
/// motor, no un dato de la nota en sí.
enum class PitchQuality {
    Fast,      // dsp::InterpolationMethod::Linear
    HighQuality,  // dsp::InterpolationMethod::Cubic
};

/// Referencia NO PROPIETARIA (ver docs/architecture/MEMORY_AND_OWNERSHIP.md)
/// a los datos PCM reales de un sample ya cargado en memoria. Quien
/// cargó el SoundFont es dueño de `pcmData`; Voice solo lo lee durante
/// `renderBlock`. Añadido para que Voice pueda reproducir audio real
/// — sin esto, `trigger(NoteEvent)` no tenía forma de acceder a
/// ningún dato de audio (NoteEvent solo trae un `sampleId` numérico).
struct SampleSource {
    const std::int16_t* pcmData{nullptr};
    std::size_t frameCount{0};
    std::uint32_t sampleRateHz{44100};
    std::uint8_t rootNote{60};
    std::int8_t pitchCorrectionCents{0};
    bool loopEnabled{false};
    std::uint32_t loopStartFrame{0};
    std::uint32_t loopEndFrame{0};
};

/// Contrato de una voz individual. `renderBlock` es realtime-safe:
/// sin allocación, sin locks bloqueantes, sin I/O.
class Voice {
public:
    virtual ~Voice() = default;

    virtual void trigger(const NoteEvent& event, const SampleSource& source) noexcept = 0;
    virtual void release() noexcept = 0;

    /// Configura un filtro biquad por voz (bypass por defecto hasta
    /// que se llame esto). Debe llamarse ANTES de `trigger` o entre
    /// bloques, nunca de forma concurrente con `renderBlock` de esta
    /// misma instancia (ambos se asumen invocados desde el mismo
    /// audio thread — ver docs/architecture/THREADING.md).
    /// Configura la calidad de interpolación de pitch usada por esta
    /// voz (ver `PitchQuality`). Debe llamarse ANTES de `trigger` o
    /// entre bloques, mismo contrato de threading que
    /// `setFilterCutoff`. Por defecto: `PitchQuality::HighQuality`
    /// (preserva el comportamiento previo a este método, que tenía
    /// Cubic hardcodeado).
    virtual void setPitchQuality(PitchQuality quality) noexcept = 0;

    virtual void setFilterCutoff(dsp::FilterType type, float cutoffHz, float resonance) noexcept = 0;
    virtual void disableFilter() noexcept = 0;

    /// Mezcla `frameCount` frames en `outputBuffer` (acumulando, no
    /// sobreescribiendo, para permitir mezcla polifónica aguas arriba).
    virtual void renderBlock(float* outputBuffer, std::size_t frameCount) noexcept = 0;

    virtual VoiceState state() const noexcept = 0;
};

/// Construye la implementación real: lee `SampleSource::pcmData` con
/// posición fraccional persistente entre llamadas a `renderBlock`
/// (usando dsp::interpolateSample directamente, no dsp::Resampler —
/// ver nota de diseño en dsp/resampler.hpp), aplica envolvente ADSR
/// (dsp::Envelope) con parámetros por defecto fijos, respeta looping
/// simple (forward) cuando `SampleSource::loopEnabled`, y aplica
/// opcionalmente un filtro biquad por voz (dsp::Filter, ver
/// `setFilterCutoff`) usando un buffer interno de scratch pre-asignado
/// (sin allocación en `renderBlock`). La calidad de interpolación es
/// configurable vía `setPitchQuality` (default: HighQuality/Cubic),
/// no está hardcodeada.
///
/// NO incluye todavía decodificación de generadores SF2 hacia
/// parámetros de envolvente/filtro reales — eso requiere decodificar
/// el catálogo de generadores (ver limitación documentada en
/// docs/modules/MODULES.md) y queda para una fase posterior dedicada
/// a "SF2 generator mapping". `setFilterCutoff` existe y funciona,
/// pero nada lo invoca automáticamente todavía.
std::unique_ptr<Voice> createVoice(float engineSampleRateHz);

}  // namespace olysf2sampler::sampler
