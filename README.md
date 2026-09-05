# OlySf2 Sampler

**Professional Synth/Sampler Module** — YeiViKas Digital Company

Módulo profesional de sampling y SoundFont diseñado para integrarse
posteriormente en **Olyze Music Studio** (un DAW Android desarrollado
en otro proyecto) mediante **EliNer** (el motor/API reutilizable de
YeiViKas, implementado también en ese otro proyecto).

**OlySf2 Sampler NO es una aplicación Android independiente.** No
tiene Application ID, no tiene launcher, no tiene UI de aplicación
propia. Es una librería/módulo Android (`api/`, Kotlin) respaldada por
un núcleo C++20 (`native/`) puenteado vía JNI (`jni/`).

## Estructura

- `api/` — frontera pública Kotlin del módulo (Android library, vía
  Gradle/AAR — para embeberse dentro de una app como Olyze Music
  Studio).
- `native/` — núcleo C++20: core, soundfont, samples, sampler, dsp,
  midi, audio, memory, threading, scheduler, resources, diagnostics,
  device, platform. Testeable de forma aislada, sin Android.
- `jni/` — frontera JNI mínima (solo init/shutdown/noteOn/noteOff).
- `host_adapter/` — camino ALTERNATIVO de integración: adaptador ABI C
  (`AudioModuleContract.h`) para cargarse dinámicamente vía `dlopen()`
  en un host de plugins (p.ej. `OlyzeAudioModuleHost`), sin Gradle ni
  JNI-con-clases-Kotlin. Ver `host_adapter/README.md`.
- `docs/` — arquitectura, mapa de módulos, convenciones, ADRs.
- `tests/` — por subsistema + integración + fixtures categorizadas.
- `third_party/` — dependencias externas evaluadas.

## Estado

- **Fase A** — reconversión arquitectónica completa (de aplicación
  independiente "Olyze Sf2 Creator" a módulo reutilizable). Ver
  `docs/decisions/ADR-002-reconversion-a-modulo.md`.
- **Fase B** — SoundFont Core real: `native/soundfont` implementa
  parser/writer/validator RIFF/INFO/sdta/pdta funcionales, con
  round-trip verificado campo a campo, 3 fixtures reales (1 válido + 2
  malformados) y un fuzz smoke test (4000 entradas).
- **Fase C** — Sampler core real: `native/dsp` (envolvente ADSR,
  filtro biquad, LFO, resampler) y `native/sampler` (Voice con pitch e
  interpolación cúbica reales, VoiceManager con voice-stealing,
  KeyVelocityMapping, SamplerEngine) funcionando de extremo a extremo
  — nota MIDI real produce audio real, decae a silencio tras release,
  40 notas simultáneas fuerzan voice-stealing sin crash.

- **Cierre de pendientes** — `SamplerEngine::noteOn`/`noteOff` ahora
  son realtime-safe de verdad (cola lock-free `threading::SpscQueue`
  entre control thread y audio thread, verificado bajo
  ThreadSanitizer con dos hilos reales corriendo en paralelo), y
  `Voice` ahora puede aplicar un filtro biquad por voz (verificado
  midiendo atenuación real de alta frecuencia).

Todo lo anterior verificado limpio bajo AddressSanitizer,
UndefinedBehaviorSanitizer y ThreadSanitizer según corresponda — no
solo compilado. Ver `docs/modules/MODULES.md` (secciones "Fase B",
"Fase C" y "Cierre de pendientes") para el alcance exacto y las
limitaciones que siguen abiertas.

- **Fase D** — Audio Engine real: `audio::AudioEngine` orquesta
  `platform::AudioBackend`; `platform::OboeAudioBackend` conecta con
  Oboe (Google) vía CMake `FetchContent`, descargado y compilado
  dentro del build de Android real. `jni/` ensambla
  `SamplerEngine` + `AudioEngine` + Oboe y arranca un tono de prueba
  disparable desde Kotlin (`nativeNoteOn`/`nativeNoteOff`).
  **Advertencia honesta**: Oboe no se puede descargar en el entorno
  donde se escribió este código (sin red) — `oboe_audio_backend.cpp`
  y `olysf2sampler_jni_bridge.cpp` son las únicas dos piezas de todo
  el proyecto que no se compilaron ni una vez antes de esta entrega.
  Se verifican por primera vez en CI. Ver
  `docs/architecture/AUDIO_ENGINE.md` para el detalle completo — es
  lectura obligatoria antes de asumir que esto ya suena en un
  dispositivo real.

- **Integración con Host de plugins** — `host_adapter/` implementa
  `AudioModuleContract.h` (ABI C, `dlopen()`) para cargarse como
  módulo dinámico en `OlyzeAudioModuleHost`, sin pasar por Gradle/AAR.
  Verificado con una simulación fiel del Host real (`dlopen` +
  `dlsym` + ciclo de vida completo), bajo ASan/UBSan.

**CI**: `.github/workflows/build.yml` — siete jobs: `build` (Kotlin +
núcleo C++ real vía NDK/CMake, descarga y enlaza Oboe),
`native-smoke-test` (14 módulos en host), `soundfont-core-tests`
(round-trip + fixtures + fuzz, ASan/UBSan), `dsp-and-sampler-tests`
(DSP medido + sampler end-to-end + filtro por voz + AudioEngine con
backend de prueba, ASan/UBSan), `concurrency-tests` (cola SPSC +
SamplerEngine bajo concurrencia real, ThreadSanitizer),
`host-adapter-tests` (simulación fiel del Host, ASan/UBSan),
`host-adapter-android-build` (compila la `.so` real para `arm64-v8a` y
la sube como artifact descargable, lista para cargar en el Host).

**Nota**: se intentó automatizar también la verificación en emulador
Android real (`connectedDebugAndroidTest`) como job de CI, pero el
runner de esta cuenta no tiene aceleración por hardware (KVM)
disponible — el emulador nunca llega a arrancar a tiempo. Se retiró
ese job en vez de dejar un paso que falla siempre; esa verificación
queda como paso manual con un emulador/dispositivo real (ver
`docs/architecture/AUDIO_ENGINE.md`).

Próxima fase: **Fase E — Integración SoundFont↔Sampler** (cargar un
`.sf2` real vía `native/soundfont`, y usar sus samples/presets reales
en vez del tono de prueba de la Fase D, tanto en `jni/` como en
`host_adapter/`).
