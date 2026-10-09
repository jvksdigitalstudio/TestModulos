# tests/sampler

Tests reales de `native/sampler` (Fase C) usando un `SamplerEngine`
completo de extremo a extremo (mapping + voiceManager + voces reales),
no mocks:

- **real_sampler_test.cpp** — `noteOn` produce audio realmente
  no-silencioso; `noteOff` decae a silencio dentro del tiempo de
  release esperado; 40 notas simultáneas contra una polifonía de 32
  fuerzan voice-stealing sin crash; `KeyVelocityMapping` resuelve
  correctamente por rango de nota.

Compilado y ejecutado en CI bajo AddressSanitizer + UndefinedBehaviorSanitizer.
