Bytes que no son SF2 válido a nivel de estructura RIFF, para tests de
robustez del parser ante datos no confiables (ver
docs/architecture/SECURITY.md).

- **not_riff.sf2** (35 bytes) — cabecera que no es "RIFF" en absoluto.
  El parser debe rechazarlo con `ErrorCode::MalformedInput` en el
  primer chequeo, sin intentar interpretar nada más.
- **bad_riff_size.sf2** (638 bytes) — `minimal.sf2` válido con el
  campo de tamaño RIFF (bytes 4-7) sobrescrito a `0xFFFFFF`, mucho
  mayor que el buffer real. El parser debe detectar la inconsistencia
  antes de intentar leer más allá del buffer real.
- **truncated_pdta.sf2** (319 bytes) — la primera mitad exacta de
  `minimal.sf2`, cortando a mitad del chunk `pdta`. El parser debe
  fallar limpiamente al no encontrar todos los sub-chunks obligatorios,
  no leer basura ni crashear.

Los tres se generaron con un script Python de una línea a partir de
`minimal.sf2` (ver `tests/soundfont/fixture_test.cpp` para cómo se
consumen). Verificados bajo AddressSanitizer + UndefinedBehaviorSanitizer:
ninguno produce lectura fuera de rango ni comportamiento indefinido.
