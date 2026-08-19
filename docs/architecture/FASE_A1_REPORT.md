==========================================================
OLYSF2 SAMPLER
FASE A.1 — CONSOLIDATION & HARDENING REPORT
==========================================================

## 1. Resumen ejecutivo

Esta sesión NO completó las 44 secciones del prompt de Fase A.1 a
nivel profesional/comercial. Eso es trabajo real de varios días de un
equipo (CMake/ABI, JNI, host adapter, Kotlin API, sanitizers ASan/
UBSan/TSan corridos de verdad, fuzzing extendido, NDK/Android build,
auditoría noexcept completa, resampler abstraction, etc. — ninguno de
estos se tocó).

Lo que sí se hizo: se identificaron problemas reales y concretos
mediante lectura directa del código (no de la documentación, tal como
exige §1), se corrigieron 6 de ellos con cambios de código reales, y
cada corrección se verificó compilando y EJECUTANDO tests que
demuestran el bug contra el código original (cuando aplica) y su
ausencia contra el código corregido — no se declaró nada "arreglado"
sin evidencia ejecutable.

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
- **Verificado, NO era un bug** — el loop engine de `voice_impl.cpp`
  (§14: `loopLength == 0`, `loopStart >= loopEnd`) ya maneja ambos
  casos correctamente vía la guarda `loopEndFrame > loopStartFrame`
  (cae a reproducción sin loop, sin división por cero). Se agregó un
  test de regresión para dejarlo protegido, no se cambió código.

## 4. Problemas corregidos

Los 5 primeros de §3. El sexto punto de §3 (loop engine) no requería
corrección, solo verificación con test — se documenta igual porque
"verificar y confirmar que algo está bien" es tan parte de la
auditoría como corregir lo que está mal (§1: "una funcionalidad se
considera implementada solamente si existe código real, conectado Y
PROBADO").

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
- `tests/soundfont/hardening_test.cpp` — dos tests: round-trip de
  sampleLink/sampleType (antes se perdían, ahora sobreviven), y
  rechazo explícito (`ResourceLimitExceeded`) de un modelo con 70,000
  zonas en un preset (fuerza overflow del índice de 16 bits).
- `docs/architecture/FASE_A1_REPORT.md` — este informe.

## 7. Archivos eliminados

Ninguno.

## 8. Cambios arquitectónicos

Ninguno estructural mayor. Los cambios son locales a `voice_manager`,
`sampler_engine`, y `soundfont` writer/parser/model — no se tocaron
capas (JNI, host adapter, audio, platform) ni se introdujeron nuevas
dependencias entre módulos. `SamplerEngine` gana un método nuevo en su
interfaz pública (`droppedNoteEventCount`) y `createSamplerEngine` gana
un parámetro con default — ambos cambios retrocompatibles en código
fuente (no en ABI binaria: cualquier consumidor debe recompilar, pero
eso ya era cierto para todo este proyecto, que no promete ABI estable
entre módulos internos, solo en la frontera C de JNI/host adapter,
que no se tocó).

## 9. Cambios SoundFont

Ver §5-6: preservación de sampleLink/sampleType, detección de overflow
en el writer. El parser sigue sin modelar `sm24` (24-bit audio) — esto
NO se tocó en esta sesión, sigue siendo una limitación documentada
(no una regresión).

## 10. Cambios Sampler

Ver §5-6: fix de voice stealing (P0), eliminación estructural del
escenario de voz huérfana, polifonía configurable, diagnóstico de
overflow de cola.

## 11. Cambios DSP

Ninguno. `voice_impl.cpp` (loop engine, dentro del módulo sampler no
dsp) se leyó completo y se verificó con test nuevo, pero no se
modificó — ya era correcto para los casos límite de §14 auditados.
Otros aspectos de DSP (§20-21: NaN/infinity en Envelope/Filter/LFO/
Resampler directamente, resampler abstraction) no se auditaron.

## 12. Cambios Audio

Ninguno. No auditado.

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

Ninguno. No auditado. Nota: si `api/`/JNI expone en algún momento
`droppedNoteEventCount()` hacia Kotlin, sería trabajo de una fase
futura — no se tocó nada de `jni/` en esta sesión.

## 16. Cambios Host Adapter

Ninguno. Se verificó que el único call site de `createSamplerEngine`
en `host_adapter/src/olyze_module_adapter.cpp` sigue compilando sin
cambios (usa el parámetro default) y que `SamplerEngine` no tiene
otros implementadores en el repo que la nueva función pura virtual
pudiera romper (`grep` confirmó un solo `: SamplerEngine` real,
`sampler_engine_impl.cpp`). No se auditó nada más de este módulo
(§28: ABI, lifetime, thread safety, rampMs).

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
directamente con `g++ -std=c++20 -Wall -Wextra` (sin sanitizers —
tampoco están configurados/verificados en este entorno), enlazando
manualmente contra las mismas fuentes que su CMakeLists.txt real. Se
corrió el conjunto COMPLETO de tests de `sampler` y `soundfont` juntos
en la pasada final (no solo cada uno aislado), para descartar
interacciones entre los cambios:

- `tests/sampler/real_sampler_test.cpp` — PASS
- `tests/sampler/voice_filter_test.cpp` — PASS
- `tests/sampler/live_parameters_test.cpp` — PASS
- `tests/sampler/concurrent_render_test.cpp` (con `-pthread`) — PASS
- `tests/sampler/voice_manager_stale_handle_test.cpp` (nuevo) — PASS
- `tests/sampler/layer_orphan_test.cpp` (nuevo) — PASS
- `tests/sampler/queue_overflow_diagnostics_test.cpp` (nuevo) — PASS
- `tests/sampler/loop_engine_edge_cases_test.cpp` (nuevo) — PASS
- `tests/soundfont/roundtrip_test.cpp` — PASS
- `tests/soundfont/hardening_test.cpp` (nuevo) — PASS

NO ejecuté: `tests/soundfont/fixture_test.cpp`,
`tests/soundfont/fuzz_smoke_test.cpp`, `tests/audio/*`,
`tests/core/*`, `tests/threading/*`, `tests/midi/*`,
`tests/samples/*`, `tests/integration/*` — no fueron tocados por los
cambios de esta sesión, pero tampoco se re-verificaron explícitamente
contra el árbol final.

## 19. Tests nuevos

5 archivos nuevos (uno más que en la entrega anterior), listados en
§6, todos ejecutados con resultado PASS (ver §18) y — donde aplica (P0
y overflow de writer) — confirmados como capaces de FALLAR contra el
código sin corregir, para que el test tenga valor real de regresión y
no sea solo un smoke test.

## 20. Resultados de tests

Ver §18. 10/10 tests ejecutados en PASS, corridos juntos en la pasada
final. Cero regresiones detectadas en los tests preexistentes tocados
por los cambios.

## 21. Sanitizers ejecutados

Ninguno. ASan/UBSan/TSan no están configurados en este entorno de
ejecución (sin red, sin cmake). El propio `MODULES.md` del proyecto
afirma en varios lugares que partes previas del código SÍ fueron
verificadas bajo ASan/UBSan en algún momento anterior — esta sesión no
pudo confirmar ni refutar eso, y tampoco corrió sanitizers sobre el
código nuevo.

## 22. Resultados de sanitizers

N/A — no se ejecutaron.

## 23. Problemas que permanecen

Prácticamente todo el resto de las 44 secciones del prompt original:
- §9 auditoría noexcept completa.
- §10/§15 verificación exhaustiva de realtime-safety más allá de lo
  ya auditado (el filtro por bloques grandes, `kMaxFilterScratchFrames`,
  se leyó y parece correcto — degrada a sin-filtro más allá de 4096
  frames en vez de fallar, comportamiento documentado en el propio
  código — pero no se escribió test dedicado para ese caso límite).
- §11 eventos sample-accurate (frameOffset) — diseño no revisado.
- §16 parameter smoothing completo (rampMs) más allá de volumen/filtro.
- §21 abstracción de resampler.
- §23 CMake/ABI/PIC completo.
- §25 auditoría JNI.
- §26 aislamiento Oboe.
- §27 API Kotlin.
- §28 host adapter (ABI, lifetime, rampMs real) más allá del chequeo
  puntual de compatibilidad de §16 de este informe.
- §30-32 matriz completa de testing/fuzzing/sanitizers.
- `sm24` (24-bit) sigue sin modelarse (limitación documentada, no
  nueva).

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

No es `APPROVED`: la mayoría de las 44 secciones del prompt original
no se auditaron ni se tocaron en esta sesión (§23). No es
`NOT APPROVED`: los cambios que sí se hicieron son reales, verificados
con tests ejecutables que demuestran el bug (cuando existía) y su
corrección, no introdujeron regresiones detectables en el conjunto
completo de tests corrido junto en la pasada final, y no se fingió
ninguna verificación que no ocurrió (sin sanitizers, sin cmake/ctest,
documentado explícitamente en vez de omitido).

Condición para poder declarar `APPROVED` sobre Fase A.1 completa:
ejecutar §23 de este informe como backlog explícito, con acceso a un
entorno con cmake + NDK + sanitizers.
