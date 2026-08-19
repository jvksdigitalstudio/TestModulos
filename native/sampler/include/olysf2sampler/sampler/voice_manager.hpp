#pragma once
// native/sampler/polyphony — asignación y ciclo de vida de voces.
// Responsabilidad única: decidir qué voz atiende qué nota y aplicar la
// política de robado de voces (voice stealing) cuando se agota la
// polifonía disponible. No renderiza audio ni conoce DSP directamente.

#include <cstddef>
#include <memory>

#include "olysf2sampler/sampler/voice.hpp"

namespace olysf2sampler::sampler {

class VoiceManager {
public:
    virtual ~VoiceManager() = default;

    virtual void setMaxPolyphony(std::size_t maxVoices) noexcept = 0;

    /// Adquiere una voz para el evento dado (con su fuente de audio ya
    /// resuelta), aplicando voice-stealing si es necesario. Devuelve
    /// kInvalidVoiceId si no fue posible (p.ej. maxPolyphony == 0).
    ///
    /// El VoiceId devuelto identifica esta INSTANCIA de nota concreta,
    /// no solo un slot del pool. Si la voz subyacente es robada más
    /// tarde por otra nota (voice stealing), este VoiceId queda
    /// automáticamente inválido: no debe asumirse que sigue
    /// refiriéndose a "la misma voz que suena ahora en ese slot".
    virtual VoiceId acquireVoice(const NoteEvent& event, const SampleSource& source) noexcept = 0;

    /// Libera la voz identificada por `id`, si y solo si `id` sigue
    /// siendo la instancia vigente (no fue robada por otra nota desde
    /// que se adquirió). Un handle stale (voz robada mientras tanto)
    /// es un no-op seguro: releaseVoice NUNCA debe apagar una voz que
    /// pertenece a una nota distinta de la que originalmente pidió
    /// este id. Ver docs/architecture/MEMORY_AND_OWNERSHIP.md.
    virtual void releaseVoice(VoiceId id) noexcept = 0;

    /// Aplica un filtro LowPass a TODAS las voces del pool (activas e
    /// inactivas — las inactivas lo heredan cuando se disparen). Debe
    /// llamarse solo desde el audio thread (ver
    /// docs/architecture/THREADING.md, patrón de parámetros continuos):
    /// realtime-safe, sin allocación, solo recalcula coeficientes de
    /// biquad por voz.
    virtual void setFilterCutoff(float cutoffHz, float resonance) noexcept = 0;

    /// Llama a `Voice::renderBlock` de cada voz activa, mezclando todo
    /// en `outputBuffer` (acumulación, no sobreescritura). Avanza
    /// voces en Release hasta Idle y las libera automáticamente.
    /// Realtime-safe: sin allocación.
    virtual void renderBlock(float* outputBuffer, std::size_t frameCount) noexcept = 0;
};

/// Construye la implementación real: pool de voces fijo (pre-asignado
/// en `setMaxPolyphony`, sin allocación dinámica posterior), con
/// robo de voces por antigüedad (roba la voz activa más vieja) cuando
/// se agota la polifonía disponible.
std::unique_ptr<VoiceManager> createVoiceManager(float engineSampleRateHz);

}  // namespace olysf2sampler::sampler
