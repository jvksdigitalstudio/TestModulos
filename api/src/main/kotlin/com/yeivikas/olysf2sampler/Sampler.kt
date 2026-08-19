package com.yeivikas.olysf2sampler

/**
 * Contrato inicial del servicio de sampler. La UI de edición de mapeo
 * (app.editor.keymapping / velocitymapping) llama a estas operaciones;
 * la resolución real del mapeo vive en native/sampler (mapping.hpp).
 */
interface SamplerService {
    fun loadInstrument(instrument: InstrumentHandle): EngineResult<Unit>
    fun noteOn(note: MidiNote, velocity: Velocity)
    fun noteOff(note: MidiNote)
    fun setMapping(mapping: KeyVelocityMapping)
}

@JvmInline
value class InstrumentHandle(val id: String)

@JvmInline
value class MidiNote(val value: Int)

@JvmInline
value class Velocity(val value: Int)

data class KeyVelocityMapping(
    val zones: List<MappingZone> = emptyList(),
)

data class MappingZone(
    val lowNote: MidiNote,
    val highNote: MidiNote,
    val lowVelocity: Velocity,
    val highVelocity: Velocity,
    val sampleId: String,
)
