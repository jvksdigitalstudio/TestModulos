#pragma once
// native/dsp/filters — primitivas de filtrado de señal.
// Responsabilidad única: procesar muestras de audio. NO conoce voces,
// instrumentos, ni SoundFont (eso es responsabilidad de olysf2sampler::sampler).

#include <cstddef>
#include <cstdint>
#include <memory>

namespace olysf2sampler::dsp {

enum class FilterType {
    LowPass,
    HighPass,
    BandPass,
    Notch,
};

/// Contrato mínimo de un filtro de un solo polo/biquad. La implementación
/// concreta (biquad, state-variable, etc.) llega en la fase de DSP.
/// `process` debe ser realtime-safe: sin allocaciones, sin I/O.
class Filter {
public:
    virtual ~Filter() = default;

    /// `sampleRateHz` se añadió al contrato original (que no lo tenía)
    /// al implementar el filtro real: los coeficientes de un biquad
    /// son indisociables de la tasa de muestreo — no existía forma
    /// correcta de implementar `configure` sin este dato. Ver
    /// docs/architecture/FILE_AUDIT.md, nota de Fase C.
    virtual void configure(FilterType type, float cutoffHz, float resonance,
                            float sampleRateHz) noexcept = 0;

    /// Procesa `frameCount` muestras mono in-place. El buffer es
    /// responsabilidad del llamador (sampler/voice); este método no
    /// asigna memoria.
    virtual void process(float* samples, std::size_t frameCount) noexcept = 0;

    virtual void reset() noexcept = 0;
};

/// Construye la implementación real: biquad Direct Form I con
/// coeficientes RBJ Audio EQ Cookbook. `resonance` se interpreta como
/// factor Q, acotado internamente a un rango estable ([0.1, 20]).
std::unique_ptr<Filter> createBiquadFilter();

}  // namespace olysf2sampler::dsp
