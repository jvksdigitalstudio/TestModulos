# ARCHITECTURE.md — OlySf2 Sampler

> Reconversión arquitectónica: este proyecto ya NO es "Olyze Sf2
> Creator" (aplicación Android independiente). Ver
> `docs/decisions/ADR-002-reconversion-a-modulo.md` para el porqué.

## Qué es OlySf2 Sampler

Un **módulo Synth/Sampler profesional reutilizable** — no una
aplicación. Su destino final es integrarse como instrumento dentro de
**Olyze Music Studio** (un DAW Android, desarrollado en otro
proyecto), a través de **EliNer** (el motor/API reutilizable de
YeiViKas Digital Company, que se está implementando en ese mismo
proyecto de Olyze Music Studio — no aquí).

```
Olyze Music Studio
        │
        ├── Synth / Sampler / Drum / MIDI / Mixer / ...
        │         │
        │         └── OlySf2 Sampler   ← este repositorio
        │
        └── EliNer API / EliNer Core   ← otro repositorio
```

Este repositorio **no** implementa EliNer ni Olyze Music Studio.
Implementa el núcleo de sampler/SoundFont que, en el futuro, se
consumirá desde ese motor — por eso el núcleo está diseñado para no
depender de nada de Android/UI y ser 100% testeable de forma aislada.

## Estructura

```
com.yeivikas / OlySf2 Sampler
│
├── api/        Kotlin — frontera pública del módulo (Android library,
│               empaqueta la .so nativa vía Gradle/AAR)
│
├── native/     C++20 — núcleo real: core, soundfont, samples, sampler,
│               dsp, midi, audio, memory, threading, scheduler,
│               resources, diagnostics, device, platform
│
├── jni/        frontera JNI mínima (Kotlin ↔ C++), camino de
│               integración vía Gradle/AAR
│
├── host_adapter/  frontera ABI C (dlopen), camino de integración
│               ALTERNATIVO para hosts de plugins de audio
│               (OlyzeAudioModuleHost u otros) — ver
│               host_adapter/README.md
│
├── tests/      por subsistema + integration + fixtures categorizadas
│
├── docs/       arquitectura, módulos, API, convenciones, ADRs
│
└── third_party/  dependencias evaluadas
```

`jni/` y `host_adapter/` son dos fronteras DISTINTAS hacia el mismo
`native/` — ninguna depende de la otra, ambas traducen el mismo
`sampler::SamplerEngine` hacia un protocolo de integración diferente
(Gradle/AAR/JNI vs. `.so` suelta cargada con `dlopen()`). Cuál se usa
depende de quién consume el módulo: una app que lo embebe en su APK
usa `jni/`; un host de plugins que carga módulos en tiempo de
ejecución usa `host_adapter/`.

No existe `app/`. No existe Application ID. No existe launcher. No
existe formato de proyecto propio (`.olysf2c` fue eliminado junto con
la aplicación que lo definía) — el único formato de archivo relevante
para este módulo es `.sf2` (SoundFont estándar), y todavía no se
implementa su parser/writer completo (ver roadmap).

## Dirección de dependencia

```
(futuro) Olyze Music Studio → EliNer API → EliNer Core → OlySf2 Sampler
                                                              │
                                                    native/ (C++20, sin Android)
                                                              │
                                                    native/platform (única
                                                    frontera Oboe/AAudio/NDK)
```

Regla dura: **`native/` no depende de nada de Android/UI.** Se puede
compilar y probar en host (confirmado — ver informe de reconversión).
`native/platform` es la única excepción consciente: es la frontera
deliberada hacia Oboe/AAudio, y el resto de `native/` no la usa
directamente salvo a través de `native/audio`.

## Por qué se conserva la agrupación `native/`

La arquitectura propuesta para el módulo agrupa los subsistemas
(`core/`, `soundfont/`, `sampler/`, ...) como hermanos directos. Se
mantiene la agrupación técnica `native/` como padre común de esos
subsistemas C++ porque: (1) separa limpiamente el árbol CMake del
árbol Gradle/Kotlin (`api/`), evitando mezclar convenciones de build
distintas en el mismo nivel; (2) es el punto natural que
`externalNativeBuild` de Android Gradle Plugin espera referenciar. No
es una desviación de fondo de la arquitectura pedida, solo una
decisión de organización de directorios con justificación técnica.

## Reglas heredadas que se mantienen sin cambios

- Una responsabilidad → un componente (nada de archivos "Manager"
  gigantes).
- El audio callback no hace I/O, alloc dinámica ni llamadas JNI.
- `dsp/` no conoce `sampler/`; `soundfont/` no conoce UI.
- Toda dependencia externa se evalúa contra los criterios documentados
  en `third_party/README.md` antes de integrarse.

## Documentos relacionados

- `MEMORY_AND_OWNERSHIP.md` — política de ownership (RAII vs.
  referencias no propietarias, cuándo se justifica `shared_ptr`).
- `THREADING.md` — separación audio thread / worker / scheduler y qué
  puede bloquear dónde.
- `SECURITY.md` — cómo debe tratar el parser SF2 los datos no
  confiables (bounds checking, límites de recursos).
- `FILE_AUDIT.md` — catálogo de TODOs/stubs y verificación de que no
  hay código muerto, duplicado, ni dependencias circulares.
