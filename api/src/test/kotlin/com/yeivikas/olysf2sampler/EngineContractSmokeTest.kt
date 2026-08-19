package com.yeivikas.olysf2sampler

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Smoke test de la Public API de OlySf2 Sampler.
 *
 * IMPORTANTE: este archivo corre como test unitario JVM
 * (`testDebugUnitTest`), NO en un dispositivo/emulador Android. Por
 * eso NO instancia `internal.NativeOlySf2SamplerEngine` — esa clase
 * carga `libolysf2sampler.so` en su companion object
 * (`System.loadLibrary`), y una `.so` compilada para Android no puede
 * cargarse desde un JVM de host: eso produce `UnsatisfiedLinkError`,
 * no un fallo real del código. Los tests que necesiten ejercitar la
 * implementación respaldada por JNI viven en `src/androidTest/` (ver
 * `NativeOlySf2SamplerEngineInstrumentedTest.kt`), que sí corre sobre
 * un dispositivo/emulador real — y por eso mismo no forma parte del
 * job `build` de `.github/workflows/build.yml` (ese job no levanta un
 * emulador; solo `gradle :api:build`, que ejecuta unit tests JVM).
 *
 * Este archivo se limita a los tipos puros de Kotlin de la Public API,
 * sin ningún componente nativo.
 */
class EngineContractSmokeTest {

    @Test
    fun `EngineDiagnostics default is not initialized`() {
        assertFalse(EngineDiagnostics().isInitialized)
    }

    @Test
    fun `engine version placeholder is stable`() {
        assertEquals("0.1.0-scaffold", EngineDiagnostics().engineVersion)
    }

    @Test
    fun `EngineResult Success carries its value`() {
        val result = EngineResult.Success(42)
        assertTrue(result is EngineResult.Success)
        assertEquals(42, (result as EngineResult.Success).value)
    }

    @Test
    fun `EngineResult Failure carries its reason`() {
        val result = EngineResult.Failure("algo falló")
        assertTrue(result is EngineResult.Failure)
        assertEquals("algo falló", (result as EngineResult.Failure).reason)
    }
}
