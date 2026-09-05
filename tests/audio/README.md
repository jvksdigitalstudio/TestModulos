# tests/audio

Tests reales de `native/audio` (Fase D).

- **audio_engine_test.cpp** — prueba `AudioEngineImpl` (la
  orquestación real, no un mock) inyectando un `AudioBackend` de
  prueba en vez de Oboe: confirma que `start()` abre el backend con la
  configuración correcta, que el callback de render se propaga tanto
  si se configura antes como después de `start()`, que `stop()` cierra
  el backend, y que la latencia reportada viene del backend real.

Esto es todo lo que se puede verificar de `native/audio` sin un
dispositivo Android — la implementación real de Oboe
(`native/platform/src/oboe_audio_backend.cpp`) se verifica en CI, no
aquí. Ver `docs/architecture/AUDIO_ENGINE.md`.
