# tests/dsp

Tests reales de `native/dsp` (Fase C).

- **real_dsp_test.cpp** — 4 tests que MIDEN comportamiento real, no
  solo "compila y no crashea": forma de la envolvente ADSR en puntos
  conocidos de tiempo, atenuación medida de un tono de 8kHz por un
  filtro biquad LowPass con corte en 500Hz, valores exactos de un LFO
  sine en fases conocidas, y longitud de salida de un resampler 2x.

Compilado y ejecutado en CI bajo AddressSanitizer + UndefinedBehaviorSanitizer
en el job `dsp-and-sampler-tests` de `.github/workflows/build.yml`.
