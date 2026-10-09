# AUDIO_ENGINE.md — Fase D

## Qué se construyó

```
sampler::SamplerEngine::renderBlock
        ↑ (AudioRenderCallback, std::function)
audio::AudioEngine (orquestación)
        ↑ inyecta
platform::AudioBackend (interfaz)
        ↑ implementa
platform::OboeAudioBackend (real, solo Android)
        ↑ usa
Oboe (Google, vía CMake FetchContent)
        ↑ usa
AAudio / OpenSL ES (Android, elegido automáticamente por Oboe)
```

`jni/bridge/olysf2sampler_jni_bridge.cpp` ensambla toda la cadena en
`nativeInitialize`: construye un `SamplerEngine`, le carga un tono de
prueba (A4, 440Hz, con loop), construye un `AudioEngine` con el
backend real de Oboe, conecta `renderBlock` como callback de render, y
arranca el stream. `nativeNoteOn`/`nativeNoteOff` (nuevos en esta
fase) permiten disparar/soltar ese tono de prueba desde Kotlin.

## Qué se verificó de verdad (compilado + ejecutado + sanitizado)

- **`audio::AudioEngine`** (`native/audio/src/audio_engine_impl.cpp`):
  orquestación completa probada en host con un `AudioBackend` de
  prueba inyectado (`tests/audio/audio_engine_test.cpp`, bajo
  AddressSanitizer/UndefinedBehaviorSanitizer). Confirma: `start()`
  abre el backend con la config correcta (mono, el sample rate/buffer
  size pedidos), el callback de render se propaga
  correctamente tanto si se configura antes como después de
  `start()`, `stop()` cierra el backend, la latencia viene del
  backend real.
- Todo lo demás de la cadena (`SamplerEngine`, `VoiceManager`,
  `Voice`, DSP) — verificado en fases anteriores, sin cambios de
  fondo en esta fase.

## Qué NO se pudo verificar en este entorno (léase con atención)

Dos archivos de esta fase **no se compilaron ni una sola vez** en el
entorno donde se escribieron, porque ese entorno no tiene acceso de
red (no puede descargar Oboe) ni los headers JNI del NDK instalados
(el JDK presente no incluye `jni.h`):

1. **`native/platform/src/oboe_audio_backend.cpp`** — la implementación
   real de `AudioBackend` respaldada por Oboe. Escrita siguiendo la
   API pública documentada de Oboe (`AudioStreamBuilder`,
   `AudioStreamCallback`, `calculateLatencyMillis()`) con el mismo
   cuidado que el resto del proyecto, pero sin poder compilarla contra
   los headers reales de Oboe aquí.
2. **`jni/bridge/olysf2sampler_jni_bridge.cpp`** — tampoco se pudo
   compilar ni siquiera parcialmente (no hay `jni.h` disponible en
   este entorno en absoluto, ni para las partes que no tocan Oboe).

**Estas dos piezas se compilan por primera vez en CI**
(`.github/workflows/build.yml`, job `build`), que corre en un runner
de GitHub con acceso de red real y el NDK instalado — exactamente lo
que faltaba aquí. Si algo falla, el error aparecerá ahí, acotado a uno
de estos dos archivos, con un log de compilador real para diagnosticar
(el mismo proceso que ya resolvió el `UnsatisfiedLinkError` de una
fase anterior).

## Decisiones de diseño de esta fase

- **Formato mono.** `SamplerEngine::renderBlock` produce una sola
  señal (ninguna `Voice` implementa paneo estéreo todavía — `dsp::pan`
  existe y está probado, pero no está conectado a `Voice`, igual que
  pasaba con el filtro antes de cerrarse ese pendiente). El
  `AudioBackendConfig` se fuerza a `channelCount = 1` en
  `AudioEngineImpl::start`. Pasar a estéreo real es trabajo futuro
  (conectar `dsp::pan` a `Voice`, análogo a como se conectó el
  filtro).
- **Una sola instancia de motor por proceso.** El bridge JNI usa
  variables globales de módulo en vez de un handle `jlong` por
  instancia — consistente con la suposición ya documentada de "un
  solo productor" en la cola SPSC de `SamplerEngine`
  (`docs/architecture/THREADING.md`). Si se necesitan instancias
  múltiples en el futuro, se generaliza entonces, no antes (Fase 1
  §32).
- **Tono de prueba, no SF2 real.** Esta fase demuestra que el pipeline
  de audio completo funciona de extremo a extremo, no la carga de
  SoundFonts reales — eso conecta `native/soundfont` (Fase B, ya
  implementado) con `SamplerEngine::loadSample`, y es la fase
  siguiente natural (Fase E).
- **Versión de Oboe pinneada.** `FetchContent_Declare` fija
  `GIT_TAG 1.9.3` explícitamente (no `main`/`master`), para que el
  build sea reproducible y no se rompa silenciosamente si Oboe publica
  cambios incompatibles.

## Verificación en emulador/dispositivo real

`NativeOlySf2SamplerEngine` es `internal` en Kotlin: ningún código
fuera del módulo `:api` puede llamar `noteOn`/`noteOff` directamente
(y OlySf2 Sampler no tiene app/launcher desde donde probarlo a mano).
La única forma real de confirmar en un dispositivo/emulador que Oboe
abre el stream y que el tono de prueba suena es correr el test
instrumentado que sí vive dentro del módulo:

```
gradle :api:connectedDebugAndroidTest
```

(requiere un emulador/dispositivo conectado vía `adb`; no hay
`gradlew` committeado — mismo motivo que en CI: sin red para generar
el `.jar` binario del wrapper).

**Por qué esto NO corre en CI (decisión, no descuido):** se intentó
automatizar con un emulador headless en GitHub Actions
(`reactivecircus/android-emulator-runner`) y falló de forma
consistente con `x86_64 emulation may not work without hardware
acceleration!` / `Timeout waiting for emulator to boot` — el runner de
esta cuenta no tiene KVM disponible, así que el emulador intenta
arrancar por software puro y nunca llega a tiempo. Esto no es
arreglable ajustando el YAML (probé caché de AVD, opciones de arranque
más ligeras, más tiempo de espera — el problema es la falta de
aceleración por hardware del runner, no la configuración). Se retiró
el job en vez de dejar un paso de CI que falla siempre y desperdicia
minutos.

**La verificación real en dispositivo/emulador queda como paso
manual**, usando un emulador con aceleración real (como el que ya
existe en `OlyzeAudioModuleHost`, o cualquier emulador local de
Android Studio con hardware acceleration habilitada) — que además es
el único lugar donde de verdad se puede *escuchar* el resultado, cosa
que un emulador headless de CI tampoco podría ofrecer aunque arrancara.

## Próxima verificación pendiente (la haces tú al hacer push)

1. ¿Compila `oboe_audio_backend.cpp` contra el Oboe real descargado
   por FetchContent?
2. ¿Compila `olysf2sampler_jni_bridge.cpp` contra `jni.h` del NDK?
3. ¿Enlaza todo junto en `libolysf2sampler.so`?
4. (Más allá de CI, en un dispositivo/emulador real) ¿Se escucha
   realmente el tono de 440Hz al llamar `nativeNoteOn`?
