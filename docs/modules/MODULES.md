# MODULES.md — OlySf2 Sampler

## native/ (C++20, host-buildable, sin dependencias de Android salvo platform/)

| Módulo | Responsabilidad | Depende de | Estado |
|---|---|---|---|
| core | Result<T>, Error, versión | (ninguno) | Implementado (tipos base + versión real) |
| memory | Pools de buffers realtime-safe | core | Contrato |
| threading | Contrato audio thread / worker thread + `SpscQueue` real (cola lock-free control→audio) | core | **Parcial** — `SpscQueue` implementado y verificado bajo ThreadSanitizer; `AudioThreadHandle`/`WorkerThread` siguen siendo contrato |
| scheduler | Orquestación de tareas async | core, threading | Contrato |
| diagnostics | Logging estructurado, métricas atómicas | core | Contrato |
| device | Capacidades del dispositivo | core | Contrato |
| resources | Carga de recursos del núcleo | core | Contrato |
| midi | Eventos y gestión MIDI | core | Contrato |
| samples | I/O, procesamiento offline, loop points | core | Contrato |
| soundfont | Modelo, parser, writer, validación, serialización SF2 | core | **Implementado (Fase B)** — RIFF/INFO/sdta/pdta real: parser, writer, validator funcionando con round-trip verificado, fuzz-tested bajo ASan/UBSan (4000 entradas, 0 crashes) |
| dsp | Filtros, envolventes, LFO, resampling, interpolación, ganancia, pan | core | **Implementado (Fase C)** — filtro biquad (RBJ), envolvente ADSR, LFO, resampler, interpolación/ganancia/pan: todo real, verificado con tests que MIDEN comportamiento (atenuación de frecuencia, forma de envolvente, valores de LFO) |
| platform | Única frontera hacia Oboe/AAudio/NDK | core | **Parcial (Fase D)** — contrato + `OboeAudioBackend` real vía FetchContent (solo Android; no compilable/verificable en este entorno de desarrollo sin red, ver AUDIO_ENGINE.md) |
| audio | Orquestación del stream de audio | core, platform | **Implementado (Fase D)** — `AudioEngine` real, verificado en host con backend inyectado (ASan/UBSan) |
| sampler | Voces, polifonía, mapeo, modulación | core, dsp | **Implementado (Fase C)** — Voice (pitch real vía interpolación cúbica, looping, envolvente ADSR), VoiceManager (pool fijo + voice-stealing por antigüedad), KeyVelocityMapping, SamplerEngine, todo verificado end-to-end bajo ASan/UBSan (nota real -> audio real -> release -> silencio; 40 notas simultáneas sin crash) |

"Contrato" = interfaz abstracta compilable, sin implementación
concreta todavía. Ver `docs/decisions/` para cuándo se implementa cada
uno (roadmap Fase B en adelante).

## api/ (Kotlin)

Expone: `OlySf2SamplerEngine`, `AudioSessionService`, `SamplerService`,
`SoundFontService`, `EngineDiagnostics`. La implementación real
(`internal.NativeOlySf2SamplerEngine`) es interna al módulo — ningún
consumidor externo (tests, o en el futuro Olyze Music Studio vía
EliNer) debe instanciarla directamente.

## jni/

`bridge/olysf2sampler_jni_bridge.{hpp,cpp}` — únicamente creación/
destrucción del núcleo y conversión de tipos. Prohibido: DSP, parsing,
sampler, procesamiento de samples dentro de este archivo.

## Qué se eliminó (ya no existe en este repo)

`app/` completo: UI Compose, navegación, ViewModels, formato de
proyecto `.olysf2c`, preferencias de aplicación, Application ID,
launcher. Ver ADR-002 para el razonamiento.

## Fase B — alcance real de `native/soundfont` (parser/writer/validator)

**Implementado y verificado** (round-trip real, ASan/UBSan limpio, fuzz smoke test):
- Parsing/escritura completos de RIFF → LIST INFO/sdta/pdta → phdr/pbag/pmod/pgen/inst/ibag/imod/igen/shdr.
- Metadata INFO (isng/INAM/IPRD/ICOP/ICMT/ISFT).
- Referencias cruzadas Preset→Instrument (generador 41) e Instrument→Sample (generador 53), con validación de rango.
- Datos PCM de 16 bits (`sdta`/`smpl`) preservados byte a byte en el round-trip.
- Todo acceso a bytes del archivo pasa por `SafeByteReader` (bounds-checked) — nunca lectura fuera de rango, confirmado con AddressSanitizer.

**Limitaciones conocidas de esta fase** (no implementado todavía, documentado para no fingir cobertura completa):
- `sm24` (audio de 24 bits) — se ignora si está presente; solo se soporta 16 bits.
- `wSampleLink`/`sfSampleType` — **preservados desde Fase A.1** (parser los captura, writer los reescribe tal cual; ver `SampleHeader` en `model.hpp`), pero su SEMÁNTICA (resolución de pares estéreo enlazados, ROM sample linking) todavía no se interpreta en ninguna capa superior — se conservan como datos opacos, no se pierden en round-trip.
- Generadores de rango (`keyRange`/`velRange`, que empaquetan lo/hi en un mismo campo de 16 bits) se preservan como valor crudo de 16 bits sin decodificar lo/hi por separado — suficiente para round-trip fiel, insuficiente todavía para que el futuro sampler interprete rangos de nota/velocity.
- El validador no verifica todavía el catálogo completo de ~58 operadores de generador de la spec, solo las dos referencias estructurales (instrument/sampleID) y los rangos de sample.
- Sin soporte de exportación a versiones SF2 distintas de 2.01 (el writer siempre declara `ifil` 2.01).
- **Fase A.1 (hardening):** el writer ahora detecta explícitamente si un modelo excede los índices de 16 bits de bag/gen/mod (demasiadas zonas/generadores/moduladores) y devuelve `ErrorCode::ResourceLimitExceeded` en vez de truncar/wrappear silenciosamente los índices — antes de esta fase, un modelo suficientemente grande habría producido un archivo SF2 corrupto sin ningún error.

Estas limitaciones no afectan la seguridad ante datos no confiables (ver `SECURITY.md`) — afectan fidelidad/cobertura funcional, y quedan para fases posteriores del roadmap (Sampler core necesitará decodificar keyRange/velRange, por ejemplo).

## Fase C — alcance real de `native/dsp` y `native/sampler`

**Implementado y verificado** (compilado y ejecutado bajo ASan/UBSan,
no solo diseñado):
- `dsp::Envelope` (ADSR con rampas lineales, release proporcional al
  nivel en el momento de noteOff).
- `dsp::Filter` (biquad Direct Form I, coeficientes RBJ Audio EQ
  Cookbook, 4 tipos). El contrato se extendió con un parámetro
  `sampleRateHz` que faltaba en el diseño original (los coeficientes
  de un biquad son indisociables de la tasa de muestreo).
- `dsp::Lfo` (acumulador de fase, 4 formas de onda).
- `dsp::Resampler` (lineal/cúbico, para conversión de buffer completo
  — distinto del mecanismo que usa `Voice` para su propia
  reproducción continua, ver nota en `resampler.hpp`).
- `sampler::Voice`: pitch real (MIDI note vs. root note + fine tune +
  pitch correction → ratio de semitonos), interpolación cúbica,
  looping forward, envolvente ADSR aplicada por muestra.
- `sampler::VoiceManager`: pool fijo pre-asignado (sin allocación en
  el hot path), voice-stealing por antigüedad cuando se agota la
  polifonía. **Fase A.1 (P0, corregido):** `VoiceId` ahora empaqueta
  slot + generación, no solo un índice de slot desnudo — un handle
  obtenido para una nota que luego es robada por voice-stealing queda
  automáticamente stale y `releaseVoice()` sobre ese handle es un
  no-op seguro, en vez de apagar por error la voz que ahora pertenece
  a otra nota. Ver `tests/sampler/voice_manager_stale_handle_test.cpp`
  (reproduce el bug contra la implementación anterior y confirma el
  fix).
- `sampler::SamplerEngine`: la polifonía máxima ahora es un parámetro
  configurable de `createSamplerEngine` (antes: constante hardcodeada
  `32`), y el límite de capas rastreables por nota ya no es una cota
  arbitraria (antes `8`, fija) sino que se dimensiona igual a la
  polifonía máxima configurada — elimina estructuralmente el
  escenario de "voz huérfana" (una capa disparada más allá del límite
  que un `noteOff` posterior ya no podía apagar).
- `sampler::KeyVelocityMapping`: resolución por rango de nota/velocity
  (recorrido lineal — documentado como suficiente para el número de
  zonas típico de un preset SF2).
- `sampler::SamplerEngine`: fachada que ata todo lo anterior;
  `loadSample`/`noteOn`/`noteOff`/`renderBlock`/`setMapping`/
  `setMasterVolume`/`setFilterCutoff` (estos dos últimos, parámetros
  continuos en tiempo real vía `std::atomic`, ver
  `docs/architecture/THREADING.md`).

## Cierre de pendientes (post-Fase C)

Dos de las limitaciones originales de esta fase quedaron cerradas:

- **`noteOn`/`noteOff` ahora SÍ son realtime-safe end-to-end.**
  `SamplerEngine` usa `threading::SpscQueue` (cola lock-free) para
  cruzar del control thread al audio thread; `renderBlock` es 100%
  alloc-free incluyendo el drenado de comandos. Verificado bajo
  ThreadSanitizer con dos hilos reales ejecutándose en paralelo (ver
  `docs/architecture/THREADING.md`).
- **`Voice` ahora SÍ puede aplicar un filtro biquad por voz.**
  `Voice::setFilterCutoff(type, cutoffHz, resonance)` usa un buffer de
  scratch pre-asignado (sin allocación en `renderBlock`). Verificado
  midiendo atenuación real de contenido de alta frecuencia (rugosidad
  de señal: 319.6 → 6.8 con LowPass a 500Hz sobre una mezcla de
  200Hz+10kHz).
- **`SamplerEngine` ahora expone parámetros continuos reales:
  `setMasterVolume` y `setFilterCutoff`** (el segundo aplica a todas
  las voces vía `VoiceManager::setFilterCutoff`, broadcast al pool
  completo). Verificado midiendo el efecto real en el audio (volumen
  0.25 → pico ~1/4 del original; filtro 500Hz → misma atenuación
  medida que el filtro por voz) y, además, verificado end-to-end a
  través del ABI C de `host_adapter/` con `dlopen()` real — moviendo
  `setParameter(Master Volume, 0.1)` a través del ABI, exactamente
  como lo haría un Host de plugins, el audio medido cae de verdad.

**Limitaciones que siguen abiertas** (no implementado todavía,
documentado para no fingir cobertura completa):
- `Voice` sigue usando parámetros de envolvente ADSR FIJOS, y nada
  llama a `setFilterCutoff` automáticamente todavía — ambos dependen
  de decodificar el catálogo completo de generadores SF2 hacia
  parámetros reales, que es una fase posterior dedicada (ver
  limitación ya documentada para `soundfont`). El mecanismo para
  aplicarlos ya existe y está probado; falta la fuente de datos real
  que los alimente.
- `PitchQuality` (Fast/HighQuality) existe como enum en el contrato
  pero `Voice` siempre usa interpolación cúbica — el selector no está
  cableado todavía (documentado, no roto: simplemente no configurable
  aún).
- Voice-stealing es "por antigüedad" (roba la voz disparada hace más
  tiempo), no por nivel de amplitud actual ni prioridad de nota — una
  política más sofisticada es una mejora futura, no un defecto de esta
  fase.
- El productor de la cola SPSC se asume único (un solo control
  thread). Si en el futuro varios hilos necesitan llamar
  `noteOn`/`noteOff` concurrentemente (p.ej. UI + MIDI externo a la
  vez), hace falta serializar esas llamadas antes de la cola (un mutex
  fino en el productor, o una cola MPSC) — no está implementado porque
  no hay todavía un caso de uso real con múltiples productores.


## Fase D — Audio Engine real

Pipeline completo: `jni/` construye `SamplerEngine` + `AudioEngine`
con `OboeAudioBackend` real, carga un tono de prueba (A4, 440Hz), y
arranca el stream — `nativeNoteOn`/`nativeNoteOff` (nuevos en Kotlin,
sobre `NativeOlySf2SamplerEngine`) lo disparan/sueltan.

**Verificado de verdad:** `audio::AudioEngine` (orquestación completa,
host + ASan/UBSan, con un `AudioBackend` de prueba inyectado — ver
`tests/audio/`).

**NO verificado en este entorno** (documentado explícitamente, no
escondido): `native/platform/src/oboe_audio_backend.cpp` y
`jni/bridge/olysf2sampler_jni_bridge.cpp` no se compilaron ni una vez
aquí (sin red para Oboe, sin `jni.h` disponible siquiera). Se
compilan por primera vez en CI. Ver
`docs/architecture/AUDIO_ENGINE.md` para el detalle completo,
incluidas las decisiones de diseño (mono, instancia única de motor
por proceso, tono de prueba en vez de SF2 real).

Próxima fase natural: conectar `native/soundfont` (Fase B) con
`SamplerEngine::loadSample` para reproducir SoundFonts reales en vez
del tono de prueba.

## Integración con hosts de plugins — `host_adapter/`

Camino de integración alternativo a `jni/`, para hosts que cargan
módulos vía `dlopen()` con un ABI C plano (`AudioModuleContract.h`),
como `OlyzeAudioModuleHost`. No usa Gradle/AAR ni JNI.

**Verificado de verdad**: build real de `libolysf2sampler_module.so`
con `olyze_module_entry` exportado (confirmado con `nm -D`), simulación
fiel del Host real (`dlopen`/`dlsym`/ciclo de vida completo) bajo
AddressSanitizer + UndefinedBehaviorSanitizer — incluyendo medir que
`process()` produce silencio real sin eventos y audio real con
`NOTE_ON`, que decae a silencio real tras `NOTE_OFF`, y que los 3
parámetros reales (Master Volume, Filter Cutoff, Filter Resonance)
cambian el audio medido cuando se ajustan a través del ABI —
**confirmado además funcionando dentro del proceso real de
`OlyzeAudioModuleHost` en un dispositivo Android** (el usuario cargó
la `.so`, vio "48000Hz · 192fr · 2ch · 0 err" en los diagnósticos de
Oboe, y escuchó el tono de prueba).

**No verificado en este entorno**: la compilación cruzada a
`arm64-v8a` (sin NDK aquí) — se hace en CI
(`host-adapter-android-build`, sube la `.so` como artifact).

Ver `host_adapter/README.md` para el detalle completo, incluidas las
limitaciones documentadas (sin sample-accurate scheduling, 0
parámetros expuestos todavía, sin presets).
