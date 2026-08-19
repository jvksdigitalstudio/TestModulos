# tests/soundfont

Tests reales de `native/soundfont` (Fase B — implementación funcional
del núcleo SoundFont).

- **roundtrip_test.cpp** — construye un `SoundFontModel` en memoria,
  lo escribe con `Sf2Writer`, lo vuelve a parsear con `Sf2Parser`, y
  compara campo a campo. También prueba que `Sf2Validator` detecta una
  referencia cruzada corrupta a propósito, y que el parser rechaza una
  cabecera RIFF corrupta y un archivo truncado.
- **fixture_test.cpp** — igual que arriba pero leyendo los fixtures
  reales de `tests/fixtures/` desde disco, no construidos en memoria.
- **fuzz_smoke_test.cpp** — 2000 buffers aleatorios + 2000 mutaciones
  del fixture válido; la única propiedad exigida es que el parser
  nunca crashee (verificable bajo ASan/UBSan, que es como corre en CI).

Los tres se compilan y ejecutan en CI en el job `soundfont-core-tests`
de `.github/workflows/build.yml`, con `-fsanitize=address,undefined`.
