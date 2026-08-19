# THREADING.md

Estrategia de threading pedida en §23 del encargo de Fase A.

## Hilos previstos

| Hilo | Responsabilidad | Puede bloquear/asignar memoria | Contrato |
|---|---|---|---|
| Audio thread | Ejecuta `AudioRenderCallback` / `SamplerEngine::renderBlock` | **No** — nunca | `threading::AudioThreadHandle` |
| Worker thread(s) | Parsing SF2, I/O de disco, carga/decodificación de samples, preprocesamiento | Sí | `threading::WorkerThread` |
| Scheduler | Encola trabajo diferido/retrasado hacia workers | Sí (indirectamente) | `scheduler::TaskScheduler` |
| Integración (Kotlin/futuro consumidor) | Llama a la API pública, nunca al audio thread directamente | Sí | fuera de `native/` |

## Regla dura (heredada de Fase 1 §13, sigue vigente)

El audio thread nunca hace: I/O, parsing, allocación dinámica no
pre-reservada, locks bloqueantes, llamadas JNI, logging pesado. Todo
lo que requiera cualquiera de eso ocurre en un worker thread y le
llega al audio thread ya preparado (buffers pre-cargados desde
`memory::BufferPool`, voces ya resueltas desde
`sampler::KeyVelocityMapping`).

## Comunicación entre hilos

Los contratos actuales (`AudioThreadHandle::setCallback`,
`WorkerThread::enqueue`) son la interfaz; el mecanismo de paso de
datos seguro entre worker → audio thread (colas lock-free/SPSC) es una
decisión de implementación que se toma en la fase donde se construya
`sampler::VoiceManager` real — no se fija de más aquí para no
sobrediseñar antes de tener el caso de uso concreto (§32).

## Casos de uso previstos que dependen de esta separación

- **Carga de sample**: worker thread lee el archivo, decodifica,
  normaliza (`samples::processing`) → publica el buffer resultante de
  forma segura → el audio thread solo lo consume cuando ya está listo.
  Nunca se decodifica un sample dentro de una voz activa.
- **Streaming futuro** (samples más grandes que la RAM disponible): el
  contrato de `WorkerThread` está pensado para soportarlo sin cambiar
  la interfaz del audio thread, pero no se implementa en esta fase.
- **Offline rendering** (exportar un preset renderizado a WAV sin
  reproducir en vivo): reutilizaría `SamplerEngine::renderBlock` desde
  un worker thread en vez del audio thread real — el contrato ya lo
  permite porque `renderBlock` no asume qué hilo lo llama, solo que
  quien lo llama respeta las reglas de realtime-safety si es el audio
  thread real.

## Parámetros continuos: std::atomic, no la cola SPSC

Los eventos de nota (`noteOn`/`noteOff`) son discretos y necesitan
resolución (mapping, búsqueda de sample) antes de convertirse en un
comando — por eso usan la cola SPSC. Los parámetros continuos
(`setMasterVolume`, `setFilterCutoff`) no necesitan resolución: son un
único valor float que reemplaza al anterior. Para esos, `SamplerEngine`
usa `std::atomic<float>` directamente:

- `setMasterVolume`/`setFilterCutoff` (control thread, p.ej. el hilo de
  UI de un Host de plugins moviendo un slider) hacen un
  `store(..., memory_order_relaxed)` — wait-free, sin la cola.
- `renderBlock` (audio thread) hace un `load(...)` al inicio de cada
  bloque; si el valor cambió respecto al último aplicado, recalcula lo
  necesario (coeficientes de biquad) UNA vez por cambio real, no en
  cada bloque sin necesidad.

Es el patrón estándar en audio en tiempo real para parámetros
continuos (más liviano que una cola para un solo valor mutable).
Verificado bajo ThreadSanitizer junto con el resto de
`concurrent_render_test.cpp`.

## Estado real (cerrado)

`sampler::SamplerEngine::noteOn`/`noteOff` (implementación real desde
Fase C) empujan comandos POD a `threading::SpscQueue` (cola lock-free
de un productor/un consumidor, `native/threading/spsc_queue.hpp`).
`renderBlock` drena esa cola al inicio de cada bloque y aplica los
comandos usando únicamente operaciones ya garantizadas alloc-free
(`VoiceManager::acquireVoice`/`releaseVoice`, arrays de tamaño fijo
indexados por nota MIDI). Toda la resolución que sí puede asignar
memoria (`KeyVelocityMapping::resolve`, búsqueda en el registro de
samples) ocurre en el control thread ANTES de construir el comando,
nunca dentro de `renderBlock`.

Verificado de forma real, no solo por diseño: `tests/threading/spsc_queue_test.cpp`
(200.000 items entre dos hilos reales, orden estricto, cero pérdidas)
y `tests/sampler/concurrent_render_test.cpp` (5.000 eventos de nota
disparados desde un hilo mientras otro renderiza 2.000 bloques de
audio en paralelo) — ambos compilados y ejecutados bajo
**ThreadSanitizer**, que reportaría cualquier data race real entre los
dos hilos. Limpio.

## Explícitamente NO se construye en esta fase

Una arquitectura de threading "genérica" con pools de hilos
configurables, actors, o frameworks de concurrencia — sería
sobreingeniería sin un caso de uso real todavía (§32).
