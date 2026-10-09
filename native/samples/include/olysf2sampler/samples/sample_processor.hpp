#pragma once
// native/samples/processing — procesamiento real de audio de
// samples (normalización, trim, fade). Responsabilidad única: DSP
// offline sobre buffers de sample completos (a diferencia de
// olysf2sampler::dsp, que opera en tiempo real dentro del sampler).
//
// La edición VISUAL (selección de rango en pantalla, waveform) NO vive
// en este módulo — pertenecerá al futuro anfitrión (Olyze Music
// Studio); este módulo solo ejecuta el procesamiento que esa UI
// solicite a través de la Public API.

#include "olysf2sampler/samples/sample_io.hpp"

namespace olysf2sampler::samples {

class SampleProcessor {
public:
    virtual ~SampleProcessor() = default;

    virtual void normalize(PcmBuffer& buffer, float targetPeak) noexcept = 0;
    virtual void trim(PcmBuffer& buffer, std::size_t startFrame, std::size_t endFrame) noexcept = 0;
};

}  // namespace olysf2sampler::samples
