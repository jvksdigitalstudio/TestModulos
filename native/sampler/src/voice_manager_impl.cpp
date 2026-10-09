#include "olysf2sampler/sampler/voice_manager.hpp"

#include <cstdint>
#include <limits>
#include <vector>

#include "olysf2sampler/dsp/filter.hpp"

namespace olysf2sampler::sampler {

namespace {

// El pool es fijo y reutiliza slots (voice stealing): un VoiceId debe
// distinguir "la voz que yo disparé" de "otra voz que ahora ocupa el
// mismo slot porque la mía fue robada" (ver docs/architecture/
// MEMORY_AND_OWNERSHIP.md, caso "stale voice handle"). Para eso el
// handle empaqueta slot + generación en un único uint32_t opaco:
//
//   bits [0..15]  -> slot index + 1 (0 == kInvalidVoiceId)
//   bits [16..31] -> generación del slot en el momento de acquireVoice
//
// Cada vez que un slot es (re)asignado — tanto para una voz Idle como
// por robo — su generación se incrementa. releaseVoice() solo actúa
// si la generación del id coincide con la generación ACTUAL del slot;
// si no coincide, el handle es stale (la voz fue robada mientras
// tanto) y la llamada es un no-op seguro, tal como exige la Fase A.1
// §12. Esto es válido únicamente hasta 65535 voces simultáneas
// (kMaxSupportedVoices), muy por encima de cualquier polifonía SF2
// realista; setMaxPolyphony() lo satura defensivamente si se pide más.
constexpr std::uint32_t kSlotBits = 16;
constexpr std::uint32_t kSlotMask = 0xFFFFu;
constexpr std::size_t kMaxSupportedVoices = kSlotMask;  // slot+1 debe caber en 16 bits

VoiceId makeVoiceId(std::size_t slot, std::uint16_t generation) noexcept {
    return (static_cast<VoiceId>(generation) << kSlotBits) |
           (static_cast<VoiceId>(slot + 1) & kSlotMask);
}

// Devuelve el slot codificado (0-based) o kInvalidVoiceId-equivalent
// (std::size_t máximo) si el id es inválido en su forma (slot 0).
std::size_t decodeSlot(VoiceId id) noexcept {
    std::uint32_t rawSlot = id & kSlotMask;
    if (rawSlot == 0) {
        return static_cast<std::size_t>(-1);
    }
    return static_cast<std::size_t>(rawSlot - 1);
}

std::uint16_t decodeGeneration(VoiceId id) noexcept {
    return static_cast<std::uint16_t>(id >> kSlotBits);
}

class FixedPoolVoiceManager final : public VoiceManager {
public:
    explicit FixedPoolVoiceManager(float engineSampleRateHz)
        : engineSampleRateHz_(engineSampleRateHz) {}

    void setMaxPolyphony(std::size_t maxVoices) noexcept override {
        // Pre-asigna el pool completo aquí (fuera del audio thread por
        // contrato — ver doc de la interfaz). Ninguna otra operación de
        // esta clase asigna memoria dinámica.
        if (maxVoices > kMaxSupportedVoices) {
            maxVoices = kMaxSupportedVoices;  // límite del formato del handle, no arbitrario
        }
        voices_.clear();
        triggerOrder_.clear();
        generation_.clear();
        voices_.reserve(maxVoices);
        triggerOrder_.reserve(maxVoices);
        generation_.reserve(maxVoices);
        for (std::size_t i = 0; i < maxVoices; ++i) {
            voices_.push_back(createVoice(engineSampleRateHz_));
            triggerOrder_.push_back(0);
            generation_.push_back(0);
        }
    }

    VoiceId acquireVoice(const NoteEvent& event, const SampleSource& source) noexcept override {
        if (voices_.empty()) {
            return kInvalidVoiceId;
        }

        std::size_t targetSlot = voices_.size();  // sentinel: "no encontrado"

        // Primera pasada: cualquier voz Idle disponible.
        for (std::size_t i = 0; i < voices_.size(); ++i) {
            if (voices_[i]->state() == VoiceState::Idle) {
                targetSlot = i;
                break;
            }
        }

        // Si no hay ninguna Idle: roba la de menor triggerOrder_ (la
        // más antigua disparada), sea cual sea su estado actual.
        if (targetSlot == voices_.size()) {
            std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
            for (std::size_t i = 0; i < voices_.size(); ++i) {
                if (triggerOrder_[i] < oldest) {
                    oldest = triggerOrder_[i];
                    targetSlot = i;
                }
            }
        }

        if (targetSlot >= voices_.size()) {
            return kInvalidVoiceId;
        }

        voices_[targetSlot]->trigger(event, source);
        triggerOrder_[targetSlot] = nextTriggerOrder_++;
        // Nueva ocupación del slot (Idle->Playing o robo): nueva
        // generación. Cualquier VoiceId emitido antes para este slot
        // queda automáticamente stale a partir de aquí.
        ++generation_[targetSlot];
        return makeVoiceId(targetSlot, generation_[targetSlot]);
    }

    void releaseVoice(VoiceId id) noexcept override {
        if (id == kInvalidVoiceId) {
            return;
        }
        std::size_t slot = decodeSlot(id);
        if (slot >= voices_.size()) {
            return;
        }
        if (decodeGeneration(id) != generation_[slot]) {
            // Handle stale: la voz que este id nombraba fue robada y
            // el slot pertenece ahora a otra nota. No hacer nada —
            // liberar aquí apagaría la nota equivocada.
            return;
        }
        voices_[slot]->release();
    }

    void setPitchQuality(PitchQuality quality) noexcept override {
        // Mismo patrón de broadcast que setFilterCutoff (ver comentario
        // ahí abajo).
        for (auto& voice : voices_) {
            voice->setPitchQuality(quality);
        }
    }

    void setFilterCutoff(float cutoffHz, float resonance) noexcept override {
        // Broadcast a TODO el pool (activas e inactivas): las voces son
        // objetos persistentes reutilizados (creados una sola vez en
        // setMaxPolyphony), así que una voz inactiva que reciba esto ya
        // lo tiene aplicado la próxima vez que se dispare — no hace
        // falta reaplicar en acquireVoice().
        for (auto& voice : voices_) {
            voice->setFilterCutoff(olysf2sampler::dsp::FilterType::LowPass, cutoffHz, resonance);
        }
    }

    void renderBlock(float* outputBuffer, std::size_t frameCount) noexcept override {
        if (outputBuffer == nullptr) {
            return;
        }
        for (auto& voice : voices_) {
            if (voice->state() != VoiceState::Idle) {
                voice->renderBlock(outputBuffer, frameCount);
            }
        }
    }

private:
    float engineSampleRateHz_;
    std::vector<std::unique_ptr<Voice>> voices_;
    std::vector<std::uint64_t> triggerOrder_;
    std::vector<std::uint16_t> generation_;  // generación actual por slot, ver comentario arriba
    std::uint64_t nextTriggerOrder_{1};
};

}  // namespace

std::unique_ptr<VoiceManager> createVoiceManager(float engineSampleRateHz) {
    return std::make_unique<FixedPoolVoiceManager>(engineSampleRateHz);
}

}  // namespace olysf2sampler::sampler
