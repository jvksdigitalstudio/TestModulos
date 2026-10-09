# host_adapter/

Adaptador que hace que OlySf2 Sampler sea cargable como plugin de
audio por **OlyzeAudioModuleHost** (o cualquier otro host que hable el
mismo ABI C) vía `dlopen()`, sin necesitar Gradle/AAR/JNI-con-clases-Kotlin.

## Por qué existe esto además de `api/`+`jni/`

`api/` + `jni/` (Fase D) exponen OlySf2 Sampler como librería Android
consumida vía Gradle + `System.loadLibrary` desde una clase Kotlin
específica — el camino correcto si algún día esto se embebe
directamente dentro de una app (Olyze Music Studio, vía EliNer).

`host_adapter/` es un camino **distinto y más simple**: produce una
`.so` independiente que un host de plugins carga dinámicamente en
tiempo de ejecución, sin recompilar nada, mediante un ABI C plano
(`AudioModuleContract.h`). Es exactamente el patrón de plugins de
audio real (VST, AU) y es lo que `OlyzeAudioModuleHost` espera.

Ambos caminos consumen el mismo `sampler::SamplerEngine` ya
implementado — ninguno reimplementa audio/DSP, solo traducen hacia un
protocolo de integración distinto.

## Estructura

```
host_adapter/
├── contracts/
│   ├── AudioModuleContract.h   # copia exacta del contrato del Host
│   └── README.md               # procedencia y regla de sincronización
├── src/
│   └── olyze_module_adapter.cpp   # el adaptador real
├── tests/
│   └── host_simulation_harness.cpp  # dlopen() real, simula al Host
└── CMakeLists.txt
```

## Qué SÍ está verificado (compilado + ejecutado + sanitizado, aquí mismo)

A diferencia de `jni/`+Oboe (Fase D), **este adaptador no necesita
`jni.h` ni Oboe** — es ABI C puro, el Host es quien posee el audio
real. Por eso se pudo verificar por completo en este entorno:

- Compilación limpia (`-Wall -Wextra -Wpedantic`).
- Build real de `libolysf2sampler_module.so`, con `olyze_module_entry`
  exportado y visible (confirmado con `nm -D`).
- Simulación fiel del Host real: `dlopen()` de la `.so`, `dlsym()` del
  símbolo exacto, recorrido completo del ciclo de vida
  (`create→initialize→prepare→process→reset→shutdown→destroy→dlclose`).
- `process()` sin eventos produce silencio real (medido); `process()`
  con `NOTE_ON` produce audio real (medido) en ambos canales de
  salida; `NOTE_OFF` decae a silencio real tras el release.
- Todo lo anterior, además, limpio bajo AddressSanitizer +
  UndefinedBehaviorSanitizer.
- `getParameterInfo`/`setParameter`/`getParameter` para los 3
  parámetros reales, incluido un id inválido rechazado limpio con
  `OLYZE_ERR_INVALID_ARGUMENT` — y, crucialmente, `setParameter`
  llamado a través del ABI real cambia el pico de audio medido
  (0.24 → 0.02 al bajar Master Volume a 0.1), no solo "acepta la
  llamada sin hacer nada".

## Qué NO está verificado

- No se probó dentro del proceso real de `OlyzeAudioModuleHost` (eso
  requiere el APK del Host corriendo en un dispositivo/emulador) — el
  arnés de pruebas de aquí es la simulación más fiel posible sin eso,
  pero no es el Host real.
- La compilación cruzada a `arm64-v8a` (la `.so` que de verdad
  cargaría el Host en un teléfono) no se pudo ejecutar en este
  entorno (sin NDK aquí) — se hace en CI, ver
  `.github/workflows/build.yml`, job `host-adapter-android-build`.

## Limitaciones documentadas del adaptador (no del `SamplerEngine` en sí)

- Sin sample-accurate scheduling: todos los eventos de un bloque de
  `process()` se aplican al inicio del bloque, ignorando
  `frameOffset` — `SamplerEngine` no soporta todavía disparar una nota
  en una muestra exacta dentro de un bloque.
- 3 parámetros reales expuestos vía `getParameterCount`/
  `getParameterInfo`/`setParameter`/`getParameter`: **Master Volume**
  (0.0–2.0, default 1.0), **Filter Cutoff** (200–20000 Hz), **Filter
  Resonance** (0.1–20 Q) — conectados a `SamplerEngine::setMasterVolume`/
  `setFilterCutoff`, verificados moviendo el "slider" a través del ABI
  real y midiendo el cambio en el audio de salida. Sin rampa suave
  (`rampMs` del contrato se ignora todavía) — cambios bruscos pueden
  sonar como un "zipper" audible; documentado, no resuelto.
- Sin sistema de presets (`loadPreset`/`savePreset` devuelven
  `OLYZE_ERR_UNSUPPORTED`).
- `PITCH_BEND`/`MOD_WHEEL`/`SUSTAIN`/`CC` se ignoran silenciosamente
  (no producen error, pero tampoco hacen nada todavía).
- Sigue sonando el tono de prueba de 440Hz (Fase D), no un SoundFont
  real — la integración SoundFont↔Sampler es la fase siguiente.
