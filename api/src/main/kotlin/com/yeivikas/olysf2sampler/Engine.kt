package com.yeivikas.olysf2sampler

/**
 * Frontera pública de OlySf2 Sampler. Contrato mínimo y extensible.
 * Quien consuma este módulo (Olyze Music Studio, vía EliNer, o tests
 * locales) solo debe conocer este paquete `com.yeivikas.olysf2sampler`;
 * nunca los internals de `native/` ni tipos JNI.
 */
interface OlySf2SamplerEngine {
    fun initialize(config: EngineConfig): EngineResult<Unit>
    fun shutdown()
    fun diagnostics(): EngineDiagnostics
}

/**
 * Configuración de arranque del núcleo. Se mantiene deliberadamente
 * mínima en esta fase; se ampliará cuando exista audio real.
 */
data class EngineConfig(
    val preferredSampleRate: Int? = null,
)

/** Resultado uniforme para operaciones de la Public API. */
sealed class EngineResult<out T> {
    data class Success<T>(val value: T) : EngineResult<T>()
    data class Failure(val reason: String, val cause: Throwable? = null) : EngineResult<Nothing>()
}
