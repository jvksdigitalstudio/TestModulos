# tests/threading

Tests reales de `native/threading` (cierre de pendiente documentado en
Fase C: `SamplerEngine::noteOn`/`noteOff` no eran realtime-safe).

- **spsc_queue_test.cpp** — dos hilos reales (no simulados): un
  productor empuja 200.000 enteros, un consumidor los extrae;
  verifica orden estricto FIFO, cero pérdidas. Compilado y ejecutado
  bajo **ThreadSanitizer** (no solo ASan/UBSan) — es la herramienta
  correcta para detectar data races reales entre hilos, que ASan no
  detecta.
