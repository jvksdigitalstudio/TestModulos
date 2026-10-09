package com.yeivikas.olysf2sampler.internal

import com.yeivikas.olysf2sampler.EngineConfig
import com.yeivikas.olysf2sampler.EngineDiagnostics
import com.yeivikas.olysf2sampler.EngineResult
import com.yeivikas.olysf2sampler.OlySf2SamplerEngine

/**
 * Implementación de [OlySf2SamplerEngine] respaldada por el núcleo nativo
 * vía JNI. Es `internal`: ningún consumidor (tests locales hoy; Olyze
 * Music Studio, vía EliNer, en el futuro) instancia esta clase
 * directamente — todos consumen la interfaz pública
 * `OlySf2SamplerEngine`. Esto permite reemplazar la implementación (o
 * usar un fake en tests) sin tocar código consumidor.
 *
 * Fase D: `nativeInitialize` ahora construye un pipeline de audio real
 * de extremo a extremo (SamplerEngine + AudioEngine + Oboe, ver
 * `docs/architecture/AUDIO_ENGINE.md`) y carga un tono de prueba de
 * 440Hz. `noteOn`/`noteOff` permiten dispararlo/soltarlo — son una
 * superficie de demostración/prueba, no la API de notas definitiva
 * (esa llega junto con la integración SoundFont↔Sampler de una fase
 * posterior). El backend real de Oboe no se pudo compilar en el
 * entorno donde se escribió este código (sin red para descargarlo) —
 * se verifica por primera vez en CI, ver el documento arriba
 * mencionado antes de asumir que esto ya suena en un dispositivo real.
 */
internal class NativeOlySf2SamplerEngine : OlySf2SamplerEngine {

    companion object {
        init {
            System.loadLibrary("olysf2sampler")
        }
    }

    private var initialized = false

    override fun initialize(config: EngineConfig): EngineResult<Unit> {
        val ok = nativeInitialize(config.preferredSampleRate ?: 0)
        initialized = ok
        return if (ok) {
            EngineResult.Success(Unit)
        } else {
            EngineResult.Failure(
                "nativeInitialize() devolvió false: fallo al abrir el stream de audio " +
                    "(Oboe) o al construir el núcleo. Ver logcat / " +
                    "jni/bridge/olysf2sampler_jni_bridge.cpp.",
            )
        }
    }

    override fun shutdown() {
        if (initialized) {
            nativeShutdown()
            initialized = false
        }
    }

    override fun diagnostics(): EngineDiagnostics = EngineDiagnostics(isInitialized = initialized)

    /** Dispara el tono de prueba cargado en `initialize()`. Ver nota de clase. */
    fun noteOn(midiNote: Int, velocity: Int) {
        if (initialized) {
            nativeNoteOn(midiNote, velocity)
        }
    }

    /** Suelta el tono de prueba disparado con [noteOn]. */
    fun noteOff(midiNote: Int) {
        if (initialized) {
            nativeNoteOff(midiNote)
        }
    }

    private external fun nativeInitialize(preferredSampleRate: Int): Boolean
    private external fun nativeShutdown()
    private external fun nativeNoteOn(midiNote: Int, velocity: Int)
    private external fun nativeNoteOff(midiNote: Int)
}
