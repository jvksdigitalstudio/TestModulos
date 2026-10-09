==========================================================
OLYSF2 SAMPLER
FASE A.1 — CONSOLIDATION & HARDENING REPORT
==========================================================

## 1. Resumen ejecutivo

Esta sesión NO completó las 44 secciones del prompt de Fase A.1 a
nivel profesional/comercial. Sigue faltando trabajo real (JNI/Oboe con
NDK real, API Kotlin, resampler abstraction, frameOffset sample-accurate,
rampMs real, fuzzing extendido más allá del smoke test existente).

Lo que sí se hizo: se identificaron problemas reales y concretos
mediante lectura directa del código, se corrigieron 8 de ellos con
cambios de código reales, y cada corrección se verificó compilando y
EJECUTANDO tests — incluyendo, en esta última pasada, bajo
**ASan+UBSan+TSan reales** (disponibles en este entorno vía g++, algo
que esta misma sesión había asumido incorrectamente que no estaba
disponible en una versión anterior de este informe — corregido más
abajo) y una **integración end-to-end real del host adapter** vía
`dlopen()` de la `.so` compilada. No se declaró nada "arreglado" sin
evidencia ejecutable, y cuando esta sesión se equivocó (dos veces: la
estructura del zip entregado, y la disponibilidad de sanitizers), se
corrigió explícitamente en vez de dejarlo pasar.

## 2. Estado inicial

Proyecto real de ~3500 líneas de C++ (native/), 23 CMakeLists, 11
archivos de test preexistentes, más API Kotlin, JNI bridge y host
adapter. No es un prototipo débil: el parser RIFF ya usaba bounds
checking (SafeByteReader), overflow-safe multiplication, y la cola de
comandos control→audio ya era SPSC lock-free con justificación
explícita de realtime-safety. La disciplina general del código era
alta; los problemas encontrados son puntuales, no sistémicos.

## 3. Problemas encontrados (auditoría real, con evidencia de código)

- **P0** — `VoiceId` en `voice_manager_impl.cpp` era solo
  `slot_index + 1`, sin generación. Un `noteOff` tardío sobre una nota
  cuya voz había sido robada (voice stealing) apagaba silenciosamente
  la voz de la nota que ahora ocupaba ese slot.
- **P1** — `sf2_parser_impl.cpp` leía `wSampleLink`/`sfSampleType` del
  chunk `shdr` y los descartaba (`(void)sampleLink; (void)sampleType;`).
  `sf2_writer_impl.cpp` los reescribía como constantes fijas (0,
  `monoSample`). Cualquier SF2 con sample linking (estéreo enlazado,
  ROM) perdía esos campos en un ciclo parse→write — pérdida silenciosa
  de información prohibida explícitamente por §6.
- **P1** — `sf2_writer_impl.cpp`: los cursores `bagCursor`/
  `genCursor`/`modCursor` eran `uint16_t` incrementados sin chequeo.
  Un modelo con más de 65535 zonas/generadores/moduladores acumulados
  producía wrapping silencioso de índices → archivo SF2 corrupto sin
  ningún error reportado. Prohibido explícitamente por §8.
- **P1/P2** — `sampler_engine_impl.cpp`: `kMaxLayersPerNote` era una
  constante fija (8) independiente de `kDefaultMaxPolyphony` (32, a su
  vez hardcodeada, sin API para configurarla). Una nota con más de 8
  capas simultáneas dejaba voces sonando sin registrar: un `noteOff`
  posterior no las apagaba, quedaban huérfanas hasta ser robadas por
  voice-stealing (sin relación con la intención del usuario, viola
  §13 y §17 explícitamente).
- **P2** — `SamplerEngineImpl::noteOn/noteOff` descartaban comandos en
  la cola llena con `[[maybe_unused]] bool pushed = commandQueue_.push(cmd);`
  — pérdida de eventos sin ningún diagnóstico posible, exactamente lo
  que §22 prohíbe ("NO perder eventos silenciosamente sin diagnóstico").
- **P1** — §9 (auditoría noexcept): `Sf2Parser::parse`, `Sf2Writer::write`
  y `Sf2Validator::validate` estaban marcados `noexcept` pese a que sus
  implementaciones hacen decenas de asignaciones dinámicas
  (`std::string`, `std::vector::push_back`, `reserve`, `std::ostringstream`)
  en una ruta OFFLINE (no realtime). El propio prompt cita este caso
  case casi textualmente ("Parser y writer NO son realtime... Una
  excepción que pueda terminar en std::terminate() sin una política
  explícita NO es aceptable"). Antes: una excepción real (p.ej.
  `std::bad_alloc` bajo memoria agotada) escapando de estas funciones
  causaba `std::terminate()` inmediato, sin ningún `Result::fail()`
  posible.
- **Verificado, NO era un bug** — el loop engine de `voice_impl.cpp`
  (§14: `loopLength == 0`, `loopStart >= loopEnd`) ya maneja ambos
  casos correctamente vía la guarda `loopEndFrame > loopStartFrame`
  (cae a reproducción sin loop, sin división por cero). Se agregó un
  test de regresión para dejarlo protegido, no se cambió código.
- **P2 (§38: "no crear interfaces vacías")** — `PitchQuality`
  (Fast/HighQuality) estaba declarado en `voice.hpp` desde antes de
  esta sesión, pero absolutamente nada lo consumía: `Voice::renderInto`
  tenía `dsp::InterpolationMethod::Cubic` como literal fijo. Un enum
  público sin ningún efecto es exactamente lo que §38 prohíbe. Estaba
  documentado honestamente en `MODULES.md` como limitación conocida
  (no ocultado), pero seguía siendo una interfaz vacía real.
- **P1 (§25/§28: lifetime en la frontera JNI)** — `nativeInitialize`
  en `jni/bridge/olysf2sampler_jni_bridge.cpp` reasignaba
  `g_samplerEngine`/`g_audioEngine` (variables globales) SIN detener
  primero cualquier instancia previa. Si se llama dos veces sin un
  `nativeShutdown()` de por medio (lifecycle real de Android:
  reconfiguración, un caller con bug, dos hilos llamando casi
  simultáneamente), el audio thread del backend Oboe ANTERIOR podía
  seguir ejecutando `g_samplerEngine->renderBlock()` en el momento
  exacto en que `std::unique_ptr::operator=` destruye ese objeto al
  reasignar — use-after-free real en el audio thread, no hipotético.
  NO pude compilar esto contra el NDK real (no disponible en este
  entorno); sí lo syntax-check con un stub mínimo de `jni.h` escrito a
  mano — verificación parcial, no completa, y digo esto explícitamente
  en vez de callarlo.
- **P1 (§16/§28: rampMs documentado como pendiente, pero arreglable)**
  — el contrato ABI (`AudioModuleContract.h`) declara explícitamente:
  *"the module is responsible for internal smoothing to avoid zipper
  noise"*. El host adapter recibía `rampMs` y lo ignoraba por completo
  — `setMasterVolume`/`setFilterCutoff` aplicaban un escalón completo
  de volumen/filtro en el bloque siguiente. Esto SÍ estaba documentado
  honestamente como limitación (no oculto), pero incumplía el
  contrato ABI que el propio proyecto se había impuesto.
- **Bug encontrado por ASan en mi propio test, no en el producto** —
  la primera versión de `volume_ramp_test.cpp` tenía un
  use-after-free real: el helper devolvía el `SamplerEngine` pero
  dejaba morir el `KeyVelocityMapping` (que `setMapping()` referencia
  sin ser dueño, documentado así en `sampler_engine.hpp`) al salir de
  función. Corregido manteniendo ambos vivos en el mismo scope. Se
  documenta para mostrar que los sanitizers de esta sesión encuentran
  cosas reales, no solo pasan en verde por casualidad.

## 4. Problemas corregidos

Los 5 primeros de §3, más el fix de JNI y el de rampMs (8 correcciones
totales). El punto de loop engine no requería corrección, solo
verificación con test — se documenta igual porque "verificar y
confirmar que algo está bien" es tan parte de la auditoría como
corregir lo que está mal (§1: "una funcionalidad se considera
implementada solamente si existe código real, conectado Y PROBADO").

## 5. Archivos modificados

- `native/sampler/src/voice_manager_impl.cpp` — generación empaquetada
  en `VoiceId` (slot+generación), `releaseVoice` ignora handles stale.
- `native/sampler/include/olysf2sampler/sampler/voice_manager.hpp` —
  contrato documentado explícitamente (handle stale = no-op seguro).
- `native/sampler/src/sampler_engine_impl.cpp` — polifonía configurable
  vía constructor; `voicesByNote_`/`voiceCountByNote_` pasan de
  `std::array` fijo a `std::vector` dimensionado una sola vez en el
  constructor (fuera del audio thread) al tamaño de la polifonía
  configurada, eliminando el límite fijo de capas por nota; contador
  atómico `droppedNoteEvents_` incrementado cuando `commandQueue_.push`
  falla, expuesto vía `droppedNoteEventCount()`.
- `native/sampler/include/olysf2sampler/sampler/sampler_engine.hpp` —
  `createSamplerEngine` gana parámetro `maxPolyphony` con default
  compatible (32) — ningún llamador existente rompe; interfaz gana
  método `droppedNoteEventCount()`.
- `native/soundfont/include/olysf2sampler/soundfont/model.hpp` —
  `SampleHeader` gana campos `sampleLink`/`sampleType`.
- `native/soundfont/src/sf2_parser_impl.cpp` — captura sampleLink/
  sampleType en vez de descartarlos.
- `native/soundfont/src/sf2_writer_impl.cpp` — reescribe sampleLink/
  sampleType reales; `appendZone`/`buildPresets`/`buildInstruments`
  devuelven `bool`/`optional` y detectan overflow de índice de 16 bits
  antes de truncar; `write()` devuelve `ResourceLimitExceeded` en ese
  caso en vez de un archivo corrupto silencioso.
- `docs/modules/MODULES.md` — se corrigió una afirmación que había
  quedado FALSA tras el fix de sampleLink/sampleType (antes decía
  "se leen pero se descartan"), y se documentaron los cambios de
  voice-stealing/polifonía.
- `native/CMakeLists.txt` — agregado `set(CMAKE_POSITION_INDEPENDENT_CODE ON)`
  (§23: ninguna biblioteca del árbol se compilaba con -fPIC pese a que
  todas son STATIC y terminan enlazadas dentro de dos SHARED reales,
  `jni/` y `host_adapter/` — ver §17 de este informe para el detalle
  de qué se pudo y no se pudo verificar).
- `native/soundfont/include/olysf2sampler/soundfont/parser.hpp`,
  `writer.hpp`, `validator.hpp` — quitado `noexcept` de
  `parse`/`write`/`validate` (§9: era una promesa falsa).
- `native/soundfont/src/sf2_parser_impl.cpp`, `sf2_writer_impl.cpp`,
  `sf2_validator_impl.cpp` — cada método público ahora es un límite de
  excepción explícito: llama a una función `*Impl` privada dentro de
  `try/catch`, convierte `std::bad_alloc`/`std::exception` en un
  `Result::fail`/`ValidationReport` fatal en vez de dejarlo escapar.
- `native/sampler/include/olysf2sampler/sampler/voice.hpp`,
  `voice_manager.hpp`, `sampler_engine.hpp` — `setPitchQuality` gana
  método real en las tres capas (Voice → VoiceManager →
  SamplerEngine), mismo patrón atómico que `setFilterCutoff`.
- `native/sampler/src/voice_impl.cpp`, `voice_manager_impl.cpp`,
  `sampler_engine_impl.cpp` — implementación de lo anterior; `Voice`
  ya no tiene `dsp::InterpolationMethod::Cubic` hardcodeado, usa un
  miembro `interpolationMethod_` configurable (default Cubic, mismo
  comportamiento previo si nadie llama al método nuevo).
- `jni/bridge/olysf2sampler_jni_bridge.cpp` — fix de lifetime: se
  extrajo `stopAndReleaseCurrentEngine()` (antes solo el cuerpo de
  `nativeShutdown`) y se llama también al INICIO de
  `nativeInitialize`, garantizando que cualquier instancia previa (y
  su audio thread, si seguía corriendo) esté completamente detenida
  antes de construir la nueva.
- `native/dsp/include/olysf2sampler/dsp/gain.hpp`, `native/dsp/src/gain.cpp`
  — nueva función `applyGainRamp` (rampa lineal de ganancia muestra a
  muestra), el bloque de construcción real para smoothing de
  parámetros.
- `native/sampler/include/olysf2sampler/sampler/sampler_engine.hpp`,
  `native/sampler/src/sampler_engine_impl.cpp` — `setMasterVolume`
  gana parámetro `rampMs` (default 0.0f, compatible en código fuente);
  `renderBlock` ahora rampea el volumen de verdad
  (`applyMasterVolumeWithRamp`) con estado persistente entre bloques,
  en vez de aplicar un escalón completo al inicio del bloque.
- `host_adapter/src/olyze_module_adapter.cpp` — `moduleSetParameter`
  ahora pasa `change.rampMs` de verdad a `setMasterVolume` (antes se
  ignoraba por completo pese a que el contrato ABI dice
  explícitamente que el módulo es responsable del smoothing). Filter
  cutoff/resonance siguen aplicando un escalón — documentado como
  pendiente, no fingido.

## 6. Archivos nuevos

- `tests/sampler/voice_manager_stale_handle_test.cpp` — reproduce el
  bug P0 contra una copia del código original (falla con
  `assertion idB != idA failed`, exit 134) y confirma el fix contra el
  código corregido (pasa, incluyendo control positivo de release real).
- `tests/sampler/layer_orphan_test.cpp` — 16 capas simultáneas en una
  nota (más que el viejo límite fijo de 8), confirma que `noteOff` las
  apaga a todas.
- `tests/sampler/queue_overflow_diagnostics_test.cpp` — satura la cola
  con 1000 noteOn sin drenar, confirma que `droppedNoteEventCount()`
  refleja el número real de eventos perdidos (745 en la corrida real).
- `tests/sampler/loop_engine_edge_cases_test.cpp` — `loopLength == 0`
  y `loopStart >= loopEnd` producen audio finito (sin NaN/Inf) y
  terminan en silencio limpio, no loop infinito ni corrupción.
- `tests/sampler/pitch_quality_test.cpp` — confirma con audio real
  (una nota con pitch fraccional, forma de onda con contenido armónico)
  que `Fast` y `HighQuality` producen salidas medible y
  consistentemente distintas — prueba que el enum llegó a moverse de
  verdad, no solo que compila.
- `tests/sampler/volume_ramp_test.cpp` — dos tests: `rampMs=0` sigue
  siendo esencialmente instantáneo (compatibilidad), y `rampMs=50`
  reparte la transición de verdad a través de ~10 bloques de audio
  (salto máximo muestra-a-muestra de 0.00024 contra el escalón viejo
  de 0.488 — casi 2000x más suave). ASan encontró un use-after-free
  REAL en la primera versión de este test (un `KeyVelocityMapping` no
  propietario destruido antes de tiempo) — corregido y documentado en
  §3.
- Ampliación de `tests/dsp/real_dsp_test.cpp` (archivo preexistente,
  no nuevo): test unitario de `applyGainRamp` en aislamiento (sin
  pasar por el sampler), confirma interpolación lineal real y que
  toca ambos extremos exactamente.
- `tests/soundfont/hardening_test.cpp` — dos tests: round-trip de
  sampleLink/sampleType (antes se perdían, ahora sobreviven), y
  rechazo explícito (`ResourceLimitExceeded`) de un modelo con 70,000
  zonas en un preset (fuerza overflow del índice de 16 bits).
- `docs/architecture/FASE_A1_REPORT.md` — este informe.

## 7. Archivos eliminados

Ninguno.

## 8. Cambios arquitectónicos

Ninguno estructural mayor en el sentido de "nuevas capas o
dependencias entre módulos", pero esta vez SÍ se tocaron JNI y host
adapter (corrección respecto a versiones anteriores de este informe,
que decían "no se tocaron capas JNI, host adapter"): un fix de
lifetime en `jni/bridge/` (ver §3, §15) y el wiring real de `rampMs`
en `host_adapter/` (ver §3, §16). Ninguno de los dos introduce
dependencias nuevas entre capas — el fix de JNI es puramente interno a
ese archivo (factoriza teardown ya existente), y el de rampMs usa
exactamente la API de `SamplerEngine` que ya existía, solo dejó de
descartar un valor que ya estaba disponible. `SamplerEngine` gana tres
métodos nuevos en su interfaz pública (`droppedNoteEventCount`,
`setPitchQuality`, y `setMasterVolume` gana un parámetro) y
`createSamplerEngine` gana un parámetro con default — todos
retrocompatibles en código fuente (no en ABI binaria: cualquier
consumidor debe recompilar, pero eso ya era cierto para todo este
proyecto, que no promete ABI estable entre módulos internos, solo en
la frontera C de JNI/host adapter en sí, cuyo `AudioModuleContract.h`
no cambió).

## 9. Cambios SoundFont

Ver §5-6: preservación de sampleLink/sampleType, detección de overflow
en el writer, exception-safety real en parser/writer/validator (§9 del
prompt). El parser sigue sin modelar `sm24` (24-bit audio) — esto NO
se tocó en esta sesión, sigue siendo una limitación documentada (no
una regresión).

## 10. Cambios Sampler

Ver §5-6: fix de voice stealing (P0), eliminación estructural del
escenario de voz huérfana, polifonía configurable, diagnóstico de
overflow de cola, `PitchQuality` cableado de punta a punta (antes
interfaz vacía), `rampMs` real para volumen maestro.

## 11. Cambios DSP

Nueva función `dsp::applyGainRamp` (`gain.hpp`/`gain.cpp`) — rampa
lineal de ganancia muestra a muestra, el bloque de construcción real
para el fix de `rampMs` de §16. Verificada con un test unitario propio
en `tests/dsp/real_dsp_test.cpp` (interpolación lineal real, toca
ambos extremos exactamente), además de indirectamente por
`volume_ramp_test.cpp` a través del sampler completo.

`voice_impl.cpp` (loop engine, dentro del módulo sampler no dsp) se
leyó completo y se verificó con test nuevo, pero no se modificó — ya
era correcto para los casos límite de §14 auditados. Otros aspectos de
DSP (NaN/infinity en Envelope/Filter/LFO/Resampler directamente,
`dsp::Resampler` como clase — bandlimited/polyphase/sinc) no se
auditaron ni se tocaron.

## 12. Cambios Audio

Ninguno directo. `native/audio/src/audio_engine_impl.cpp` se leyó para
confirmar que `AudioEngine::stop()` detiene el backend de forma
síncrona ANTES de retornar — esa lectura es lo que sustenta la
corrección del fix de lifetime de JNI en §15 (si `stop()` no fuera
síncrono, ese fix sería insuficiente). No se modificó nada en
`native/audio/`.

## 13. Cambios Threading

`SpscQueue` en sí (`spsc_queue.hpp`) NO se tocó — sigue devolviendo
`bool` desde `push()`, que es lo correcto (single responsibility: la
cola informa éxito/fracaso, no decide qué hacer con un fallo). Lo que
se corrigió fue el CALL SITE en `sampler_engine_impl.cpp`, que antes
descartaba ese `bool` con `[[maybe_unused]]` y ahora lo cuenta. Esto
resuelve el punto específico de §22 sobre pérdida silenciosa de
eventos; el resto de §22 (política de overflow más allá de contar,
stress test productor/consumidor dedicado más allá del
`concurrent_render_test.cpp` ya existente) no se tocó.

## 14. Cambios Memory/Ownership

Ninguno estructural nuevo respecto del informe anterior.
`voicesByNote_`/`voiceCountByNote_` pasaron de `std::array` a
`std::vector`, pero el vector se dimensiona una única vez en el
constructor (fuera del audio thread, mismo contrato que
`voiceManager_->setMaxPolyphony`) y nunca se realoja después — el
comentario en el código lo documenta explícitamente para que quede
claro que esto NO reintroduce allocación en el audio thread.

## 15. Cambios JNI

`jni/bridge/olysf2sampler_jni_bridge.cpp` — fix de lifetime real (ver
§3): `stopAndReleaseCurrentEngine()` extraído como función compartida
y llamado también al inicio de `nativeInitialize`, no solo en
`nativeShutdown`. Esto cierra una ventana real de use-after-free si el
método se invoca dos veces sin shutdown de por medio.

**Verificación honesta**: NO tengo NDK ni `jni.h` real en este
entorno. Verifiqué el cambio con un stub de `jni.h` escrito a mano
(tipos mínimos: `jint`, `jboolean`, `jobject`, `JNIEnv`, las macros
`JNIEXPORT`/`JNICALL`/`JNI_TRUE`/`JNI_FALSE`) compilado con
`-fsyntax-only` junto a los headers reales de `native/audio`,
`native/platform`, `native/sampler`, `native/dsp`. Esto atrapa errores
de sintaxis y de tipos en la lógica C++ del bridge, pero NO es
equivalente a compilar contra el NDK real — no verifica nada
específico de JNI real (firma exacta de `JNIEnv`, convenciones de
llamada, ABI de Android) más allá de lo que mi stub simplificado
modela. Lo digo así de explícito para no exagerar la cobertura de esta
verificación.

## 16. Cambios Host Adapter

**Fix de desincronización ABI real con el Host (esta sesión, tras
recibir `OlyzeAudioModuleHost.zip`):** el Host había bumpeado
`OLYZE_MODULE_ABI_VERSION_MAJOR` de 1 a 2 (agregó `structSize` como
primer campo de `OlyzeModuleDescriptor`, un cambio real de layout
binario) y la copia vendorizada en `host_adapter/contracts/` se había
quedado en la versión vieja — el Host rechazaba el módulo con
`OLYZE_ERR_UNSUPPORTED` ("Module descriptor is smaller than this Host
expects"). Ver `host_adapter/contracts/README.md` para el detalle
completo.

Fix:
1. `AudioModuleContract.h` reemplazado por copia byte-a-byte de la
   versión real del Host (verificado con `diff`, cero diferencias).
2. `olyze_module_adapter.cpp`: se agregó `d.structSize =
   sizeof(OlyzeModuleDescriptor);` en `olyze_module_entry()`.
3. `host_adapter/tests/host_simulation_harness.cpp`: se corrigió un
   gap real en nuestro propio test — el chequeo de `abiVersionMajor`
   comparaba contra la MISMA macro del header vendorizado usado para
   compilar el módulo, una comprobación tautológica que nunca podía
   fallar (ni siquiera con `structSize` mal seteado). Ahora también
   verifica `structSize >= sizeof(OlyzeModuleDescriptor)`, replicando
   el chequeo real de `ModuleLoader::load()` del Host.

**Verificación — la más fuerte de toda esta sesión**: no reimplementé
el chequeo del Host, usé su `ModuleLoader.cpp`/`Logger.cpp`/
`DiagnosticsRingBuffer.cpp` REALES Y SIN MODIFICAR (compilados con un
stub mínimo de `<android/log.h>`, ya que ese único include no se usa
activamente en el `.cpp`) para cargar nuestro `.so`:
- Contra el `.so` viejo (contrato pre-fix): `ModuleLoader::load()`
  real del Host lo rechaza con `OLYZE_ERR_UNSUPPORTED` — reproduce
  exactamente el error de tu captura de pantalla.
- Contra el `.so` con el fix: `ModuleLoader::load()` real del Host lo
  acepta, reporta `structSize=168` (`>=168` esperado),
  `abiVersionMajor=2`, `prepare()` OK, `process()` con `NOTE_ON`
  produce audio real, `unload()` sin crash.
- Confirmé además que el `.so` viejo, leído con el struct nuevo,
  malinterpreta sus primeros bytes: el campo que antes era
  `abiVersionMajor` (=1) queda leído como si fuera `structSize`,
  produciendo un valor pequeño que correctamente falla la
  comprobación — una demostración real (no solo teórica) del peligro
  exacto que `structSize` existe para prevenir.

Reconfirmé también (recompilando desde cero) que el resto del host
adapter (ciclo de vida completo vía `dlopen()` propio, `rampMs` real,
etc. — ver más abajo) sigue funcionando con el contrato nuevo.

`host_adapter/src/olyze_module_adapter.cpp` — `moduleSetParameter`
ahora pasa `change.rampMs` de verdad a `setMasterVolume` en vez de
ignorarlo (ver §3, §10, §11). `rampMs <= 0` se trata como "sin rampa"
(0.0f) en vez de producir un valor negativo sin sentido. Filter
Cutoff/Resonance siguen aplicando un escalón completo — implementar
smoothing ahí requeriría interpolar coeficientes de biquad (más
complejo y arriesgado que una ganancia escalar), y queda fuera del
alcance de este fix: documentado como pendiente, no fingido como
resuelto.

Auditoría/verificación de esta sesión (y de la anterior, reconfirmada
tras el reset del filesystem): se recompiló `libolysf2sampler_module.so`
real con el fix de `rampMs` incluido, y se corrió
`host_simulation_harness.cpp` contra ella (dlopen/dlsym real) — el
ciclo de vida completo (create/initialize/prepare/process con eventos
de nota reales/setParameter incluyendo Master Volume/reset/shutdown/
destroy/dlclose) sigue pasando limpio, incluyendo la medición real de
que `setParameter(Master Volume, 0.1)` reduce el audio medido
(0.2429 -> 0.0233). Se verificó también que el único call site directo
de `createSamplerEngine` en este archivo sigue compilando sin cambios
(usa parámetros default) y que `SamplerEngine` no tiene otros
implementadores en el repo que las nuevas funciones puras virtuales
pudieran romper.

NO se auditó: ABI stability a través de versiones (solo se probó la
versión actual contra sí misma), lifetime bajo múltiples
`prepare()`/`reset()` consecutivos más allá de lo que el harness ya
cubre, ni smoothing real para Filter Cutoff/Resonance.

## 17. Cambios CMake/Build

Se agregaron entradas de test nuevas a:
- `tests/sampler/CMakeLists.txt` (4 tests nuevos)
- `tests/soundfont/CMakeLists.txt` (1 test nuevo)

Se corrigió un gap real de §23 (confirmado por `grep` — CERO
ocurrencias de `POSITION_INDEPENDENT_CODE`/`-fPIC` en los 23
CMakeLists.txt del proyecto antes de este cambio): todas las
bibliotecas nativas son `STATIC` y dos targets `SHARED` reales
(`jni/CMakeLists.txt` → `olysf2sampler`, `host_adapter/CMakeLists.txt`
→ `olysf2sampler_module`) las enlazan directa o transitivamente. Se
agregó `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` una sola vez en
`native/CMakeLists.txt`, que CMake propaga automáticamente a todo
target definido en ese árbol (incluyendo el Oboe descargado vía
FetchContent bajo `ANDROID`).

**Verificación honesta de este punto en particular:** intenté
reproducir el fallo de link real (código objeto no-PIC enlazado en una
biblioteca compartida) con un repro mínimo en este entorno (Ubuntu
24.04, GCC 13). El repro NO falló — este GCC concreto está compilado
con `--enable-default-pie`, lo que enmascara el problema para código
trivial incluso pasando `-fno-PIC` explícito. No tengo acceso al NDK
de Android (el toolchain real de este proyecto) en este entorno para
confirmar el fallo exacto que §23 describe. El fix se aplicó de todas
formas porque `CMAKE_POSITION_INDEPENDENT_CODE ON` es la práctica
estándar recomendida sin downside conocido (no cambia el
comportamiento de binarios que ya funcionaban, y previene la clase de
fallo descrita en toolchains más estrictos) — pero quede claro: NO
verifiqué el fallo original ni la corrección contra el NDK real, solo
contra mi comprensión de por qué existe esta clase de problema y la
ausencia total de la mitigación estándar en el proyecto.

No se auditó nada más del resto de §23 (visibility, warnings-as-errors,
sanitizers como flag de build, etc.).

## 18. Tests ejecutados

Este entorno NO tiene `cmake` instalado (`/bin/sh: 1: cmake: not
found`), así que no pude correr `cmake configure/build/ctest` como
pide §24. En su lugar compilé y ejecuté cada test afectado
directamente con `g++ -std=c++20 -Wall -Wextra`, casi siempre bajo
`-fsanitize=address,undefined` (ver §21), enlazando manualmente contra
las mismas fuentes que su CMakeLists.txt real. Nota operativa: a mitad
de esta sesión el filesystem del contenedor se reinició entre turnos y
perdí la copia de trabajo local — restauré desde el último zip
entregado y reapliqué los cambios de JNI/rampMs que se habían perdido,
volviendo a compilar y correr todo desde cero para confirmar que
seguían funcionando (no asumí que "ya estaba probado antes" sin
volver a probarlo). Se corrió el conjunto COMPLETO de tests de
`sampler`, `dsp` y `soundfont` juntos en la pasada final (no solo cada
uno aislado), para descartar interacciones entre los cambios:

- `tests/sampler/real_sampler_test.cpp` — PASS (ASan+UBSan)
- `tests/sampler/voice_filter_test.cpp` — PASS (ASan+UBSan)
- `tests/sampler/live_parameters_test.cpp` — PASS (ASan+UBSan)
- `tests/sampler/concurrent_render_test.cpp` (con `-pthread`) — PASS
  (TSan real, ver §21)
- `tests/sampler/voice_manager_stale_handle_test.cpp` (nuevo) — PASS
  (ASan+UBSan)
- `tests/sampler/layer_orphan_test.cpp` (nuevo) — PASS (ASan+UBSan)
- `tests/sampler/queue_overflow_diagnostics_test.cpp` (nuevo) — PASS
  (ASan+UBSan)
- `tests/sampler/loop_engine_edge_cases_test.cpp` (nuevo) — PASS
  (ASan+UBSan)
- `tests/sampler/pitch_quality_test.cpp` (nuevo) — PASS (ASan+UBSan)
- `tests/sampler/volume_ramp_test.cpp` (nuevo) — PASS (ASan+UBSan;
  encontró y forzó a corregir un use-after-free real en la primera
  versión del propio test, ver §3)
- `tests/dsp/real_dsp_test.cpp` (ampliado, no nuevo) — PASS (ASan+UBSan)
- `tests/soundfont/roundtrip_test.cpp` — PASS (ASan+UBSan)
- `tests/soundfont/hardening_test.cpp` (nuevo) — PASS (ASan+UBSan)
- `tests/soundfont/fixture_test.cpp` — PASS (ASan+UBSan)
- `tests/soundfont/fuzz_smoke_test.cpp` — PASS (ASan+UBSan, 4000
  entradas, sin crash)
- `host_adapter/tests/host_simulation_harness.cpp` contra
  `libolysf2sampler_module.so` real (compilada con `-fPIC -shared`) —
  PASS, ciclo de vida completo vía `dlopen()`/`dlsym()`, en modo
  normal y bajo ASan+UBSan+detección de leaks (ver §21)

NO ejecuté: `tests/audio/*`, `tests/core/*`, `tests/threading/*`,
`tests/midi/*`, `tests/samples/*`, `tests/integration/*` — no fueron
tocados por los cambios de esta sesión.

## 19. Tests nuevos

7 archivos nuevos en total a lo largo de la sesión (más la ampliación
de `real_dsp_test.cpp`, que no es un archivo nuevo), listados en §6,
todos ejecutados con resultado PASS bajo ASan+UBSan (ver §18) y —
donde aplica (P0, overflow de writer, y el propio bug de test que ASan
encontró) — confirmados como capaces de FALLAR contra el código sin
corregir, para que el test tenga valor real de regresión y no sea solo
un smoke test.

## 20. Resultados de tests

Ver §18. 16/16 tests ejecutados en PASS (incluyendo la integración
completa del host adapter), corridos juntos en la pasada final, la
mayoría bajo ASan+UBSan y uno bajo TSan real. Cero regresiones
detectadas en los tests preexistentes tocados por los cambios.

## 21. Sanitizers ejecutados

**CORRECCIÓN a la versión anterior de este informe**: dije que ASan/
UBSan/TSan "no están configurados en este entorno". Eso era
INCORRECTO — lo asumí sin probarlo, en vez de verificarlo. Los tres
SÍ están disponibles en este entorno (g++ 13 los soporta de forma
nativa, sin ningún setup adicional) y se corrieron de verdad contra
todo el código host-buildable (todo excepto `native/platform`
específico de Oboe/NDK, `jni/`, y el resto de `api/`):

- **ASan+UBSan**, los 8 tests de `tests/sampler/` (incluye
  `pitch_quality_test.cpp`, agregado después de esta corrección) —
  limpio, cero hallazgos.
- **ASan+UBSan**, los 4 tests de `tests/soundfont/` (incluyendo
  `fuzz_smoke_test.cpp`, 4000 entradas fuzz) — limpio, cero hallazgos.
- **TSan real** sobre `concurrent_render_test.cpp` (5000 eventos de
  nota concurrentes, 2000 bloques de audio, productor/consumidor real
  con `std::thread`) — limpio, cero data races detectadas. Esto
  confirma con evidencia real, no solo con el diseño, que la cola SPSC
  y el fix del VoiceManager son thread-safe bajo carga.
- **ASan+UBSan+detección de leaks**, sobre el `host_adapter` COMPLETO:
  se compiló `libolysf2sampler_module.so` real (con `-fPIC -shared`,
  la misma configuración que exige el fix de §17) y se corrió
  `host_simulation_harness.cpp` haciendo `dlopen()`/`dlsym()` real de
  la `.so`, recorriendo el ciclo de vida completo (`create` →
  `initialize` → `prepare` → `process` con eventos de nota reales →
  `setParameter` vía ABI C → `reset` → `shutdown` → `destroy` →
  `dlclose`) — limpio, cero hallazgos, cero leaks.

Lo único que sigue sin poder correrse en este entorno es lo que
depende del NDK/Android real: `native/platform/src/oboe_audio_backend.cpp`
(Oboe) y `jni/bridge/olysf2sampler_jni_bridge.cpp` (requiere `jni.h`).
Eso sigue siendo una limitación real de este entorno, no una omisión.

## 22. Resultados de sanitizers

Cero hallazgos de ASan, UBSan, ni TSan en absolutamente ningún test
corrido (sampler completo, soundfont completo incluyendo fuzzing, y la
integración completa del host adapter vía dlopen real). Esto es
evidencia fuerte y real — no una promesa — de memory-safety, ausencia
de UB, y ausencia de data races en la porción del código auditada esta
sesión y en el código preexistente que esos tests ejercitan.

## 23. Problemas que permanecen

Lo que sigue sin tocar del prompt original:
- §11 eventos sample-accurate (frameOffset) — diseño no revisado, y
  sigue siendo una limitación real end-to-end: el harness del host
  adapter también confirma que los eventos se aplican al inicio del
  bloque, no en su offset exacto.
- §16 parameter smoothing: Master Volume ya tiene rampa real (ver §3,
  §10, §16 arriba) — pero Filter Cutoff/Resonance TODAVÍA aplican un
  escalón completo. Implementar eso requeriría interpolar coeficientes
  de biquad (más riesgoso que una ganancia escalar: hay que garantizar
  estabilidad del filtro durante la interpolación), y quedó fuera del
  alcance de este fix a propósito, documentado como pendiente.
- §21 abstracción de resampler: `PitchQuality` ya está cableado
  (Fast/HighQuality → Linear/Cubic, ver arriba), pero la clase
  `dsp::Resampler` en sí (orientada a buffer completo, con métodos
  bandlimited/polyphase/sinc mencionados en el prompt) sigue sin
  implementarse — `Voice` sigue usando `dsp::interpolateSample`
  directamente (correcto, ver nota de diseño en `resampler.hpp`: el
  `Resampler` orientado a buffer no encaja con el streaming+looping
  persistente que `Voice` necesita), así que agregar más algoritmos
  ahí sería trabajo real de DSP, no solo de wiring.
- §25 JNI: se corrigió UN bug real de lifetime (ver §3, §15), pero eso
  no equivale a una auditoría completa del archivo, y sigue sin poder
  compilarse ni probarse contra el NDK real en este entorno (solo un
  stub de `jni.h` propio para syntax-check).
- §26 Oboe (`native/platform/src/oboe_audio_backend.cpp`) — NO se pudo
  compilar ni probar en este entorno, requiere NDK + descarga de Oboe
  vía FetchContent (sin red disponible aquí).
- §27 API Kotlin (`api/`) — cero líneas leídas.
- §30-31 matriz extendida de testing/fuzzing más allá del smoke test
  de 4000 entradas ya existente (útil, pero no es fuzzing continuo/
  guiado por cobertura real).
- `sm24` (24-bit) sigue sin modelarse (limitación documentada, no
  nueva).
- Auditoría noexcept del resto: `native/sampler/*`, `native/dsp/*`,
  `native/audio/*` no se revisaron función por función (ver §9 arriba
  para lo que sí se hizo).

## 24. Funcionalidades todavía pendientes

Todo lo de Fase B (SF2 → Generator → Modulator → Resolution Engine),
explícitamente fuera de alcance de A.1 por el propio prompt (§43).

## 25. Riesgos técnicos

- `droppedNoteEventCount()` es un contador acumulado desde la creación
  del engine, sin reset ni ventana temporal — útil para diagnóstico
  básico ("¿está pasando esto alguna vez?"), insuficiente para
  telemetría en producción con tasas (eventos perdidos/segundo). No se
  implementó nada más sofisticado; es un primer paso deliberadamente
  mínimo.
- El `LayerCount` (`uint16_t`) y `kMaxSupportedPolyphony` (256) en
  `sampler_engine_impl.cpp` son cotas nuevas introducidas en este fix;
  no existía antes ninguna cota explícita de polifonía máxima — 256
  es generoso pero arbitrario, debería revisarse contra un
  "performance profile" real cuando exista (§17 original).
- No se verificó el comportamiento del writer/parser ante SF2 reales
  de terceros con sample linking real (solo se probó con un modelo
  sintético mínimo en el test nuevo).

## 26. Decisiones arquitectónicas

- `VoiceId` sigue siendo `uint32_t` (no se cambió el tipo del contrato
  público) — el fix es puramente de codificación interna al slot
  dueño del bit-packing (`voice_manager_impl.cpp`), no cruza ABI.
- Se prefirió `std::vector` dimensionado una vez en el constructor
  para `voicesByNote_` en vez de mantener `std::array` de tamaño fijo
  al máximo absoluto (256×128 = 32,768 `VoiceId`s siempre reservados
  incluso con polifonía baja) — trade-off de memoria vs. flexibilidad,
  documentado en el código.
- El contador de drops se puso en `SamplerEngineImpl` (call site), no
  dentro de `SpscQueue` — la cola mantiene una única responsabilidad
  (transporte), la decisión de qué hacer con un fallo de push (y
  cómo contarlo) es del dominio que la usa, consistente con §36 del
  prompt original (una responsabilidad por clase).

## 27. Preparación para EliNer

Sin cambios relevantes — ningún tipo público nuevo cruza la frontera
JNI/host adapter; `createSamplerEngine` sigue siendo retrocompatible
en código fuente. `droppedNoteEventCount()` es un método nuevo en la
interfaz `SamplerEngine` que un futuro adaptador EliNer podría querer
exponer como métrica, pero eso no se implementó aquí.

## 28. Preparación para Olyze Music Studio

Sin cambios relevantes.

## 29. Preparación para la siguiente fase

El modelo `SampleHeader` ahora preserva `sampleLink`/`sampleType`
crudos — esto es un prerrequisito real para que una futura Fase B
pueda eventualmente interpretar sample linking/estéreo, aunque esa
interpretación en sí no se implementó aquí.

## 30. Veredicto final

**APPROVED WITH CONDITIONS**

Sigue sin ser `APPROVED`: JNI real contra NDK, Oboe, API Kotlin, y
varias secciones del prompt original (resampler como clase, filter
smoothing, frameOffset sample-accurate, matriz extendida de fuzzing)
no se completaron en esta sesión — algunas por falta de NDK/red en
este entorno concreto, otras por alcance/riesgo (interpolar
coeficientes de biquad de forma segura es más trabajo que una tarde).
No es `NOT APPROVED`: los 9 cambios que sí se hicieron son reales,
verificados con tests que compilan y corren — la mayoría bajo
ASan+UBSan reales, uno bajo TSan real, y con dos integraciones
end-to-end distintas del host adapter: nuestro propio harness, y
—la verificación más fuerte de toda la sesión— el `ModuleLoader.cpp`
REAL y SIN MODIFICAR del proyecto `OlyzeAudioModuleHost`, que
reprodujo el rechazo original (`OLYZE_ERR_UNSUPPORTED`) contra el
módulo viejo y confirmó la aceptación contra el módulo corregido,
incluyendo `process()` con audio real.

Nota sobre el proceso de esta sesión (transparencia total, incluyendo
mis propios errores):
1. El zip entregado inicialmente tenía la estructura de carpetas rota
   y le faltaba `.github/workflows/build.yml` por un patrón de
   exclusión de zip mal escrito — corregido y verificado con `diff`
   contra el original.
2. Este informe afirmó en un momento que ASan/UBSan/TSan "no estaban
   disponibles en este entorno" sin haberlo probado — falso, sí lo
   están, y se corrieron de verdad.
3. A mitad de sesión el filesystem del contenedor se reinició entre
   turnos (mi copia de trabajo local desapareció). Lo detecté, restauré
   desde el último zip entregado, y reapliqué + re-verifiqué desde
   cero (no asumí) los cambios de JNI y rampMs que se habían perdido.
4. ASan encontró un use-after-free real en mi PROPIO test nuevo
   (`volume_ramp_test.cpp`, no en el producto) — lo señalo como
   evidencia de que los sanitizers de esta sesión están encontrando
   cosas de verdad, no solo pasando en verde.
5. Se detectó y corrigió una desincronización ABI real con el Host
   (`OLYZE_MODULE_ABI_VERSION_MAJOR` 1 vendorizado vs 2 real,
   `structSize` nunca seteado) — exactamente el error que el Host
   reportaba en pantalla. Se aprovechó para corregir además un gap en
   nuestro propio harness (chequeo tautológico de `abiVersionMajor`
   que nunca podía fallar) que había dejado pasar este tipo de
   regresión sin detectar.

Condición para poder declarar `APPROVED` sobre Fase A.1 completa:
ejecutar el resto de la lista de §23 (JNI contra NDK real, Oboe, API
Kotlin, resampler como clase, filter smoothing, frameOffset
sample-accurate) con acceso a un entorno con NDK y red.
