package com.yeivikas.olysf2sampler

/**
 * Snapshot de diagnóstico del motor. Responsabilidad única: exponer
 * estado observable del motor a la app (para pantallas de debug o
 * telemetría), sin exponer internals de `native/diagnostics`.
 */
data class EngineDiagnostics(
    val isInitialized: Boolean = false,
    val engineVersion: String = "0.1.0-scaffold",
)
