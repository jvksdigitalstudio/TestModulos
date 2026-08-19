#pragma once
// native/dsp/resampling — cambio de sample rate / pitch shifting
// por resampling. Responsabilidad única: transformar un buffer de
// entrada a una tasa/pitch objetivo. No conoce el formato SF2 ni voces.
//
// NOTA DE DISEÑO (Fase C): este componente resamplea un buffer
// COMPLETO ya en memoria en una sola llamada (posición interna
// reinicia en cada `process`). No es el mecanismo que usa
// sampler::Voice para su reproducción continua entre bloques (Voice
// mantiene su propia posición fraccional persistente usando
// dsp::interpolateSample directamente, porque además necesita
// gestionar looping, algo ajeno a un resampler genérico). Este tipo
// sirve para conversiones de sample rate offline/por bloque completo.

#include <cstddef>
#include <memory>

namespace olysf2sampler::dsp {

/// Contrato de un resampler de un solo canal. La estrategia concreta
/// (lineal, sinc, polifase) se decide en la fase de implementación de DSP.
class Resampler {
public:
    virtual ~Resampler() = default;

    virtual void setRatio(double outputToInputRatio) noexcept = 0;

    /// Consume hasta `inputFrames` de `input` y escribe como máximo
    /// `outputCapacity` frames en `output`. Devuelve cuántos frames de
    /// salida se produjeron realmente. Realtime-safe: sin allocación.
    virtual std::size_t process(const float* input, std::size_t inputFrames,
                                 float* output, std::size_t outputCapacity) noexcept = 0;

    virtual void reset() noexcept = 0;
};

/// Construye la implementación real. `highQuality=true` usa
/// interpolación cúbica (dsp::InterpolationMethod::Cubic); false usa
/// lineal (más barata, suficiente para preescucha/tiempo real con
/// recursos limitados).
std::unique_ptr<Resampler> createResampler(bool highQuality);

}  // namespace olysf2sampler::dsp
