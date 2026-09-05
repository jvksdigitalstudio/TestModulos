#include "olysf2sampler/sampler/sampler_engine.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "olysf2sampler/dsp/gain.hpp"
#include "olysf2sampler/threading/spsc_queue.hpp"

namespace olysf2sampler::sampler {

namespace {

constexpr std::size_t kDefaultMaxPolyphony = 32;
constexpr std::size_t kCommandQueueCapacity = 256;
constexpr int kMidiNoteCount = 128;

// Cota superior de capas de layering rastreables por nota. Es un
// límite de diseño de la matriz `voicesByNote_` (tamaño fijo,
// requerido para que renderBlock permanezca alloc-free — ver Fase A.1
// §10), NO un límite lógico arbitrario: se fija igual al máximo de
// polifonía soportado por el motor precisamente porque, en el peor
// caso, TODAS las voces del pool podrían terminar asignadas a la
// misma nota (p.ej. un preset con muchas capas SF2 superpuestas en
// esa tecla). Con esta cota, ninguna voz adquirida queda sin rastrear
// nunca — elimina estructuralmente el riesgo de "voces huérfanas" que
// existía cuando kMaxLayersPerNote (antes 8, fijo) podía ser menor que
// maxPolyphony: una nota con más de 8 capas dejaba voces sonando sin
// que un noteOff posterior pudiera apagarlas hasta que voice-stealing
// las reclamara para otra nota. Si se necesita más polifonía que
// kMaxSupportedPolyphony, hay que subir esta constante explícitamente
// (documentado, no silencioso).
constexpr std::size_t kMaxSupportedPolyphony = 256;
using LayerCount = std::uint16_t;  // suficiente para kMaxSupportedPolyphony

enum class CommandType { Trigger, Release };

// Comando plano (POD, copiable) que cruza la frontera control-thread
// -> audio-thread por la cola lock-free. Toda la resolución que
// pueda asignar memoria (mapping->resolve, búsqueda en el registro de
// samples) ya ocurrió ANTES de construir esto, en noteOn/noteOff
// (control thread) -- drenar la cola en renderBlock solo copia datos
// y llama a operaciones ya garantizadas alloc-free (VoiceManager).
struct VoiceCommand {
    CommandType type{CommandType::Trigger};
    int midiNote{0};
    int velocity{0};
    std::uint64_t sampleId{0};
    SampleSource source{};
};

class SamplerEngineImpl final : public SamplerEngine {
public:
    explicit SamplerEngineImpl(float sampleRateHz, std::size_t maxPolyphony)
        : sampleRateHz_(sampleRateHz), voiceManager_(createVoiceManager(sampleRateHz)) {
        // Saturado defensivamente al límite estructural del diseño
        // (ver comentario de kMaxSupportedPolyphony) en vez de aceptar
        // silenciosamente un valor que rompería la invariante
        // "ninguna capa por nota queda sin rastrear".
        std::size_t effectivePolyphony =
            maxPolyphony > kMaxSupportedPolyphony ? kMaxSupportedPolyphony : maxPolyphony;
        maxPolyphony_ = effectivePolyphony;
        voiceManager_->setMaxPolyphony(effectivePolyphony);
        voicesByNote_.assign(kMidiNoteCount,
                              std::vector<VoiceId>(effectivePolyphony, kInvalidVoiceId));
        voiceCountByNote_.assign(kMidiNoteCount, 0);
    }

    void loadSample(std::uint64_t sampleId, const SampleSource& source) noexcept override {
        // Modifica un unordered_map: puede asignar memoria. Por
        // contrato debe llamarse fuera del audio thread (al cargar un
        // SoundFont) -- nunca desde dentro de renderBlock. No cruza
        // la cola lock-free porque no es un evento de nota en tiempo
        // real, es configuracion de carga.
        loadedSamples_[sampleId] = source;
    }

    void noteOn(int midiNote, int velocity) noexcept override {
        // CONTROL THREAD: aqui SI puede haber trabajo con allocacion
        // (mapping_->resolve devuelve un std::vector; loadedSamples_
        // es un unordered_map). Nada de esto entra a la cola -- solo
        // el resultado ya resuelto (VoiceCommand, POD) cruza al audio
        // thread. Esto es lo que hace que renderBlock (mas abajo) sea
        // realtime-safe de verdad, no solo de nombre.
        if (mapping_ == nullptr) {
            return;
        }
        std::vector<std::uint64_t> sampleIds = mapping_->resolve(midiNote, velocity);
        for (std::uint64_t sampleId : sampleIds) {
            auto it = loadedSamples_.find(sampleId);
            if (it == loadedSamples_.end()) {
                continue;  // sample referenciado pero nunca cargado: se ignora
            }
            VoiceCommand cmd;
            cmd.type = CommandType::Trigger;
            cmd.midiNote = midiNote;
            cmd.velocity = velocity;
            cmd.sampleId = sampleId;
            cmd.source = it->second;
            // Si la cola esta llena (productor mas rapido que el
            // consumidor de audio, situacion anomala), el comando se
            // descarta: preferible perder una nota a bloquear o
            // asignar memoria de emergencia. Pero ya NO en silencio
            // total: se cuenta para diagnóstico (§22).
            if (!commandQueue_.push(cmd)) {
                droppedNoteEvents_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    void noteOff(int midiNote) noexcept override {
        VoiceCommand cmd;
        cmd.type = CommandType::Release;
        cmd.midiNote = midiNote;
        if (!commandQueue_.push(cmd)) {
            droppedNoteEvents_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    std::uint64_t droppedNoteEventCount() const noexcept override {
        return droppedNoteEvents_.load(std::memory_order_relaxed);
    }

    void setMasterVolume(float linearGain, float rampMs) noexcept override {
        // Parámetro continuo: std::atomic en vez de la cola SPSC (ver
        // docs/architecture/THREADING.md). Llamable desde cualquier
        // hilo (típicamente el hilo de UI del Host, vía
        // OlyzeParameterChange -> moduleSetParameter).
        //
        // Nota de threading sobre target+rampMs como dos atomics
        // separados (no un único struct atómico): hay una ventana
        // teórica minúscula donde el audio thread podría leer un
        // target nuevo junto a un rampMs de una llamada anterior (o
        // viceversa) si esto se llama concurrentemente varias veces
        // en el mismo bloque. Peor caso: una rampa usa una duración
        // ligeramente distinta a la última pedida — no hay dato
        // corrupto, crash, ni valor fuera de rango. Aceptable dado que
        // los cambios de parámetro no son sample-accurate todavía
        // (§11, pendiente) y evita el costo de un lock o un
        // std::atomic<struct> no trivialmente copiable.
        masterVolumeTarget_.store(linearGain, std::memory_order_relaxed);
        masterVolumeRampMs_.store(rampMs, std::memory_order_relaxed);
    }

    void setFilterCutoff(float cutoffHz, float resonance) noexcept override {
        filterCutoffTarget_.store(cutoffHz, std::memory_order_relaxed);
        filterResonanceTarget_.store(resonance, std::memory_order_relaxed);
    }

    void setPitchQuality(PitchQuality quality) noexcept override {
        pitchQualityTarget_.store(quality, std::memory_order_relaxed);
    }

    void renderBlock(float* outputBuffer, std::size_t frameCount) noexcept override {
        // AUDIO THREAD: desde aqui hasta el final de este metodo, cero
        // asignacion de memoria. Drenar la cola solo copia PODs y
        // escribe en arrays de tamano fijo (voicesByNote_);
        // acquireVoice/releaseVoice de VoiceManager ya son alloc-free
        // (pool pre-asignado, ver voice_manager_impl.cpp).
        VoiceCommand cmd;
        while (commandQueue_.pop(cmd)) {
            applyCommand(cmd);
        }

        // Parámetros continuos: lee el valor objetivo (atomic, wait-free)
        // y solo reaplica al VoiceManager si cambió de verdad — evita
        // recalcular coeficientes de biquad en cada bloque sin necesidad.
        float wantedCutoff = filterCutoffTarget_.load(std::memory_order_relaxed);
        float wantedResonance = filterResonanceTarget_.load(std::memory_order_relaxed);
        if (wantedCutoff != appliedFilterCutoff_ || wantedResonance != appliedFilterResonance_) {
            voiceManager_->setFilterCutoff(wantedCutoff, wantedResonance);
            appliedFilterCutoff_ = wantedCutoff;
            appliedFilterResonance_ = wantedResonance;
        }

        PitchQuality wantedPitchQuality = pitchQualityTarget_.load(std::memory_order_relaxed);
        if (wantedPitchQuality != appliedPitchQuality_) {
            voiceManager_->setPitchQuality(wantedPitchQuality);
            appliedPitchQuality_ = wantedPitchQuality;
        }

        voiceManager_->renderBlock(outputBuffer, frameCount);

        applyMasterVolumeWithRamp(outputBuffer, frameCount);
    }

    void setMapping(KeyVelocityMapping* mapping) noexcept override { mapping_ = mapping; }

private:
    // Fase A.1 §16 (fix real, no solo diseño): antes, el volumen
    // maestro se aplicaba con dsp::applyGain (ganancia constante para
    // todo el bloque) usando el valor objetivo leído al INICIO del
    // bloque — un cambio de volumen producía un escalón audible en el
    // límite entre el bloque viejo y el nuevo ("zipper noise"). Ahora
    // rampea muestra a muestra usando dsp::applyGainRamp, con estado
    // de rampa persistente ENTRE bloques (masterVolumeCurrent_ /
    // masterVolumeRampFramesRemaining_ / masterVolumeRampStepPerFrame_,
    // todos audio-thread-only, sin allocación) para que un rampMs más
    // largo que un bloque siga sonando correctamente a través de
    // varias llamadas a renderBlock.
    void applyMasterVolumeWithRamp(float* outputBuffer, std::size_t frameCount) noexcept {
        float wantedTarget = masterVolumeTarget_.load(std::memory_order_relaxed);
        if (wantedTarget != masterVolumeRampToTarget_) {
            float wantedRampMs = masterVolumeRampMs_.load(std::memory_order_relaxed);
            std::size_t rampFrames =
                wantedRampMs > 0.0f
                    ? static_cast<std::size_t>(std::max(
                          1.0f, wantedRampMs / 1000.0f * sampleRateHz_))
                    : 1;  // rampMs<=0: "instantaneo" -> 1 frame, sin escalon de bloque completo
            masterVolumeRampToTarget_ = wantedTarget;
            masterVolumeRampFramesRemaining_ = rampFrames;
            masterVolumeRampStepPerFrame_ =
                (wantedTarget - masterVolumeCurrent_) / static_cast<float>(rampFrames);
        }

        std::size_t framesToRamp = masterVolumeRampFramesRemaining_ < frameCount
                                        ? masterVolumeRampFramesRemaining_
                                        : frameCount;
        if (framesToRamp > 0) {
            float rampEndGain = masterVolumeCurrent_ +
                                 masterVolumeRampStepPerFrame_ * static_cast<float>(framesToRamp);
            dsp::applyGainRamp(outputBuffer, framesToRamp, masterVolumeCurrent_, rampEndGain);
            masterVolumeCurrent_ = rampEndGain;
            masterVolumeRampFramesRemaining_ -= framesToRamp;
        }
        if (framesToRamp < frameCount) {
            // Rampa ya completada dentro de este mismo bloque: el
            // resto de las muestras usa ganancia constante en el
            // target (más barato que seguir "rampeando" hacia un
            // valor que ya se alcanzó).
            dsp::applyGain(outputBuffer + framesToRamp, frameCount - framesToRamp,
                            masterVolumeCurrent_);
        }
    }


    void applyCommand(const VoiceCommand& cmd) noexcept {
        if (cmd.midiNote < 0 || cmd.midiNote >= kMidiNoteCount) {
            return;
        }
        if (cmd.type == CommandType::Trigger) {
            NoteEvent event;
            event.midiNote = cmd.midiNote;
            event.velocity = cmd.velocity;
            event.sampleId = cmd.sampleId;
            VoiceId voiceId = voiceManager_->acquireVoice(event, cmd.source);
            if (voiceId == kInvalidVoiceId) {
                return;
            }
            LayerCount& count = voiceCountByNote_[static_cast<std::size_t>(cmd.midiNote)];
            // INVARIANTE: count nunca puede alcanzar maxPolyphony_ aquí,
            // porque el pool completo del VoiceManager tiene exactamente
            // maxPolyphony_ voces y voicesByNote_ está dimensionado a
            // maxPolyphony_ por nota — no puede haber más voces
            // simultáneas asignadas a UNA nota que el total del pool.
            // El chequeo se conserva como cinturón de seguridad
            // explícito (nunca debe fallar; si lo hace, es un bug en
            // otra parte, no una situación normal a tolerar en silencio).
            if (count < voicesByNote_[static_cast<std::size_t>(cmd.midiNote)].size()) {
                voicesByNote_[static_cast<std::size_t>(cmd.midiNote)][count] = voiceId;
                ++count;
            }
        } else {
            std::size_t noteIdx = static_cast<std::size_t>(cmd.midiNote);
            LayerCount count = voiceCountByNote_[noteIdx];
            for (LayerCount i = 0; i < count; ++i) {
                voiceManager_->releaseVoice(voicesByNote_[noteIdx][i]);
            }
            voiceCountByNote_[noteIdx] = 0;
        }
    }

    float sampleRateHz_;
    std::size_t maxPolyphony_{kDefaultMaxPolyphony};
    std::unique_ptr<VoiceManager> voiceManager_;
    KeyVelocityMapping* mapping_{nullptr};  // no propietario

    // Solo tocado desde el control thread (fuera de renderBlock).
    std::unordered_map<std::uint64_t, SampleSource> loadedSamples_;

    // Cruce control-thread -> audio-thread, lock-free.
    threading::SpscQueue<VoiceCommand, kCommandQueueCapacity> commandQueue_;

    // Diagnóstico de overflow de commandQueue_ (§22) — incrementado
    // desde el control thread (noteOn/noteOff) cuando push() falla.
    std::atomic<std::uint64_t> droppedNoteEvents_{0};

    // Parámetros continuos (volumen, filtro): std::atomic escrito desde
    // cualquier hilo, leído solo desde el audio thread dentro de
    // renderBlock. `applied*` es la copia audio-thread-only usada para
    // detectar cambios reales y no recalcular sin necesidad.
    std::atomic<float> masterVolumeTarget_{1.0f};
    std::atomic<float> masterVolumeRampMs_{0.0f};
    // Audio-thread-only: estado de la rampa de volumen, persistente
    // entre llamadas a renderBlock (ver applyMasterVolumeWithRamp).
    float masterVolumeCurrent_{1.0f};
    float masterVolumeRampToTarget_{1.0f};
    float masterVolumeRampStepPerFrame_{0.0f};
    std::size_t masterVolumeRampFramesRemaining_{0};
    std::atomic<float> filterCutoffTarget_{20000.0f};   // ~sin filtrar por defecto
    std::atomic<float> filterResonanceTarget_{0.707f};  // Q "plano" estándar
    float appliedFilterCutoff_{20000.0f};
    float appliedFilterResonance_{0.707f};
    std::atomic<PitchQuality> pitchQualityTarget_{PitchQuality::HighQuality};
    PitchQuality appliedPitchQuality_{PitchQuality::HighQuality};

    // Solo tocado desde el audio thread (dentro de renderBlock, al
    // drenar la cola). Tamaño fijado UNA vez en el constructor (fuera
    // del audio thread, igual que voiceManager_->setMaxPolyphony) y
    // nunca realojado después — por eso operator[] aquí sigue siendo
    // alloc-free pese a ser std::vector en vez de std::array: no hay
    // push_back/resize en ninguna ruta alcanzable desde renderBlock.
    std::vector<std::vector<VoiceId>> voicesByNote_;
    std::vector<LayerCount> voiceCountByNote_;
};

}  // namespace

std::unique_ptr<SamplerEngine> createSamplerEngine(float sampleRateHz, std::size_t maxPolyphony) {
    return std::make_unique<SamplerEngineImpl>(sampleRateHz, maxPolyphony);
}

}  // namespace olysf2sampler::sampler
