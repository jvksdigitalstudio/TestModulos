package com.yeivikas.olysf2sampler

/** Contrato inicial del servicio de sesión de audio (Fase 1 §H). */
interface AudioSessionService {
    fun start(config: AudioSessionConfig): EngineResult<Unit>
    fun stop()
    fun currentLatencyEstimate(): LatencyInfo
}

data class AudioSessionConfig(
    val sampleRate: Int = 48_000,
    val framesPerBuffer: Int? = null,
)

data class LatencyInfo(
    val estimatedLatencyMillis: Double,
)
