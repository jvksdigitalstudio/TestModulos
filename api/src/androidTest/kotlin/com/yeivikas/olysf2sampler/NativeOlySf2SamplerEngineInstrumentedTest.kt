package com.yeivikas.olysf2sampler

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.yeivikas.olysf2sampler.internal.NativeOlySf2SamplerEngine
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Test instrumentado: ejercita [NativeOlySf2SamplerEngine], que carga
 * `libolysf2sampler.so` de verdad — incluyendo, desde la Fase D, el
 * backend real de Oboe. Requiere un dispositivo/emulador Android
 * real — NO corre en `.github/workflows/build.yml` (ese workflow solo
 * ejecuta `gradle :api:build`, que corre unit tests JVM vía
 * `testDebugUnitTest`, no `connectedAndroidTest`).
 *
 * Para ejecutarlo hace falta `gradle :api:connectedDebugAndroidTest`
 * contra un emulador/dispositivo conectado — pendiente de añadir como
 * job de CI con `reactivecircus/android-emulator-runner` cuando se
 * quiera esa cobertura automatizada. Hasta entonces, este test (y por
 * tanto la confirmación real de que `nativeInitialize` abre un stream
 * de Oboe con éxito) solo se ejecuta manualmente en Android Studio.
 */
@RunWith(AndroidJUnit4::class)
class NativeOlySf2SamplerEngineInstrumentedTest {

    private var engine: NativeOlySf2SamplerEngine? = null

    @After
    fun tearDown() {
        engine?.shutdown()
    }

    @Test
    fun initializeOpensRealAudioStream() {
        val e = NativeOlySf2SamplerEngine()
        engine = e
        val result = e.initialize(EngineConfig(preferredSampleRate = 48000))
        // Fase D: si Oboe abre el stream correctamente en este
        // dispositivo, se espera Success. Un Failure aquí es una señal
        // real de que algo falló al abrir el audio (dispositivo sin
        // salida de audio disponible, permisos, etc.) — no un
        // resultado esperado por diseño como en fases anteriores.
        assertTrue("nativeInitialize debería abrir el stream de Oboe con éxito " +
            "en un dispositivo con salida de audio", result is EngineResult.Success)
        assertTrue(e.diagnostics().isInitialized)
    }

    @Test
    fun noteOnAndNoteOffDoNotCrash() {
        val e = NativeOlySf2SamplerEngine()
        engine = e
        e.initialize(EngineConfig(preferredSampleRate = 48000))
        e.noteOn(69, 100)  // A4, coincide con el tono de prueba cargado en nativeInitialize
        Thread.sleep(200)  // deja sonar un instante
        e.noteOff(69)
        // Sin aserción de audio (requeriría grabar la salida); la
        // propiedad que este test verifica es que la superficie JNI
        // completa (init -> noteOn -> noteOff -> shutdown) no crashea.
    }
}
