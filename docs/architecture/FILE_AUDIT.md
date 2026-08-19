# FILE_AUDIT.md

Auditoría de archivos pedida en §36 del encargo de Fase A (código
muerto, duplicado, TODOs, placeholders, stubs, clases gigantes,
nombres obsoletos, imports obsoletos, dependencias circulares).

## Código muerto
Ninguno encontrado. Todo archivo de `native/`, `api/`, `jni/` es
referenciado desde su `CMakeLists.txt`/`build.gradle.kts`
correspondiente.

## Código duplicado
Ninguno encontrado a nivel de tipo/contrato. `AudioSessionConfig`
existe tanto en `api/Audio.kt` (Kotlin, cara pública) como en
`native/audio/audio_engine.hpp` (C++, cara nativa) — esto NO es
duplicación accidental: son la misma configuración conceptual en dos
lenguajes distintos a cada lado de la frontera JNI, cada una con su
propio motivo de existir. Se documenta aquí para que quede explícito
que es intencional, no un descuido.

## Lección de CI real: unit test JVM vs. código respaldado por JNI

La primera ejecución real en GitHub Actions (`.github/workflows/build.yml`)
encontró un fallo real que ningún chequeo local con g++ podía detectar:
`EngineContractSmokeTest` (un test unitario JVM, `testDebugUnitTest`)
instanciaba `internal.NativeOlySf2SamplerEngine`, cuyo companion object
llama a `System.loadLibrary("olysf2sampler")`. Una `.so` compilada para
`arm64-v8a` no puede cargarse desde el JVM de host que ejecuta los
tests unitarios — eso produjo `UnsatisfiedLinkError`, no un error de
lógica. Corregido separando: `src/test/` solo prueba tipos Kotlin
puros (`EngineDiagnostics`, `EngineResult`); `src/androidTest/`
contiene el test que sí ejercita la clase respaldada por JNI, y ese
test requiere un dispositivo/emulador real — no corre en el job
`build` actual (que solo hace `gradle :api:build`, sin
`connectedAndroidTest`).

## TODOs y stubs (catálogo completo)

| Archivo | Qué falta | Fase que lo resuelve |
|---|---|---|
| `jni/bridge/olysf2sampler_jni_bridge.cpp` | Construcción real del núcleo en `nativeInitialize`/liberación en `nativeShutdown` | Fase B/C |
| `api/.../internal/NativeOlySf2SamplerEngine.kt` | `initialize()` siempre devuelve `Failure` (sin librería nativa enlazada) | Fase B/C |
| `api/.../SoundFont.kt` | `SoundFontModel` es un placeholder de un solo campo (`name`) en Kotlin; el modelo completo YA está implementado y verificado del lado C++ (`native/soundfont`, Fase B) — falta reflejarlo a Kotlin cuando exista el puente JNI real | Fase C (integración JNI del soundfont core) |
| `api/.../EngineContractSmokeTest.kt` (nombre de test) | El nombre del test dice "placeholder" porque `engineVersion` es un valor fijo hasta que exista build real | Fase B/C |
| 11 módulos nativos que siguen siendo solo contrato (`audio`, `sampler`, `samples`, `midi`, `memory`, `threading`, `scheduler`, `resources`, `diagnostics`, `device`, `platform` — `core` y `soundfont` ya implementados; `dsp` parcialmente implementado) | Contratos sin implementación concreta (esperado — ver `docs/modules/MODULES.md`, columna Estado) | Fase C en adelante (Sampler core, Audio Engine) |

Ningún TODO es de los prohibidos por el encargo (ninguno finge que algo
funciona; todos están marcados explícitamente).

## Placeholders

`SoundFontModel` en `api/SoundFont.kt` (Kotlin) es intencionalmente un
placeholder de un campo — el modelo real y completo ya existe en
`native/soundfont/model.hpp` (C++). Se refleja a Kotlin cuando exista
serialización JNI real que lo justifique (evita mantener dos modelos
completos sincronizados a mano antes de necesitarlo — §32).

## Nombres obsoletos / referencias a la identidad antigua
Cero — verificado por grep exhaustivo (ver informe de reconversión y
verificación adicional de esta sesión, que encontró y corrigió 3
referencias residuales a `app/` que el barrido anterior no había
detectado: `mapping.hpp`, `sample_processor.hpp`, `parser.hpp`).

## Imports obsoletos (Kotlin)
Cero detectados (heurística de uso por símbolo sobre todos los
`import` de `api/`).

## Dependencias circulares
Cero — confirmado por: (a) el grafo de `add_subdirectory`/
`target_link_libraries` en los `CMakeLists.txt` de `native/` es un DAG
por construcción (cada módulo solo enlaza hacia módulos ya declarados
antes que él en `native/CMakeLists.txt`); (b) compilación real exitosa
con g++, que habría fallado ante una dependencia circular de tipos.

## §31 — Sin límites artificiales
Verificado por grep: no existe ningún `MAX_SAMPLES`, `MAX_PRESETS`,
`MAX_INSTRUMENTS`, `MAX_ZONES` ni `MAX_VOICES` en el código. El único
límite real documentado es el de `ResourceLimitExceeded` en
`SECURITY.md`, que es una protección de seguridad ante datos no
confiables, no un tope arbitrario de diseño — y no está implementado
todavía (solo reservado como código de error).

## §32 — Anti-overengineering
Revisión de las abstracciones introducidas: cada interfaz (`Filter`,
`Envelope`, `Voice`, `Sf2Parser`, etc.) tiene un consumidor previsto
explícito documentado en `docs/modules/MODULES.md`. No se crearon
factories, wrappers de un solo uso, ni capas que solo delegan sin
aportar. La única capa que podría parecer "delega sin aportar" es
`Sf2SerializationService` (combina parser+writer) — se conserva porque
es el punto de fachada explícito que la frontera JNI necesitará para
exponer "importar/exportar" como una sola operación, no dos.

## Nota sobre el tamaño de `sf2_parser_impl.cpp` (500 líneas)

Es, con diferencia, el archivo más largo del proyecto. No viola la
regla de "sin archivos monolíticos" (Fase 1 §6/§12): todas sus
funciones hacen una sola cosa relacionada con una única
responsabilidad — interpretar el formato binario RIFF/SF2 — y cada
una es corta (`parsePhdr`, `parseInst`, `parseBag`, `parseGen`,
`parseMod`, `parseShdr`, `buildZones`, ~15-40 líneas cada una). No hay
parser + writer + DSP + MIDI mezclados en el archivo, que es lo que la
regla realmente prohíbe. Se mantiene en un solo archivo porque
dividirlo en 9 archivos de ~50 líneas cada uno (uno por tipo de
registro SF2) añadiría navegación sin añadir claridad — es exactamente
el caso que la sección "Anti-overengineering" de este mismo documento
advierte evitar: fragmentar sin necesidad real.
