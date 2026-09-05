package com.yeivikas.olysf2sampler

/** Contrato inicial del servicio SoundFont (parse/write/validate). */
interface SoundFontService {
    fun parse(source: SoundFontSource): EngineResult<SoundFontModel>
    fun write(model: SoundFontModel, target: SoundFontTarget): EngineResult<Unit>
    fun validate(model: SoundFontModel): ValidationReport
}

@JvmInline
value class SoundFontSource(val uri: String)

@JvmInline
value class SoundFontTarget(val uri: String)

/**
 * Placeholder del modelo SF2 completo (Fase 1 §11 / Fase 2 §11).
 * El modelo real (Header/Samples/Instruments/Presets/Zones/Generators/
 * Modulators) se implementa en la fase dedicada a SoundFont Core.
 */
data class SoundFontModel(
    val name: String = "",
)

data class ValidationReport(
    val isValid: Boolean,
    val issues: List<String> = emptyList(),
)
