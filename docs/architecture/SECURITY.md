# SECURITY.md

Preparación pedida en §34 del encargo de Fase A: tratar los archivos
SF2 externos como datos no confiables, aunque el parser real todavía
no exista.

## Superficie de ataque

Cualquier `.sf2` que el módulo importe puede venir de fuera de la
aplicación anfitriona (compartido por otro usuario, descargado, etc.).
El formato SF2 (RIFF) contiene tamaños y offsets declarados dentro del
propio archivo — un archivo malicioso o simplemente corrupto puede
declarar valores que no corresponden a su tamaño real.

## Categorías de error ya reservadas en el contrato (`core::ErrorCode`)

- `MalformedInput` — la estructura RIFF/INFO/SDTA/PDTA no es válida
  (chunk truncado, tamaño de chunk que excede el buffer, firma RIFF
  incorrecta).
- `ResourceLimitExceeded` — el archivo declara cantidades (nº de
  samples, presets, zonas) que excederían límites razonables de
  memoria antes de haber terminado de parsear.

Estos dos códigos existen desde ahora precisamente para que la
implementación real (Fase B) tenga dónde devolver estos casos sin
necesitar rediseñar `Result<T>`.

## Reglas obligatorias para la implementación real de `Sf2Parser` (Fase B)

1. **Todo acceso a `ByteSpan` debe validarse contra `size` antes de
   leer.** Ningún cálculo de offset+longitud puede desreferenciar sin
   comprobar primero que offset+longitud ≤ size, y esa suma debe
   calcularse de forma que no pueda desbordar (usar tipos con ancho
   suficiente o comprobar overflow explícitamente antes de sumar).
2. **Los tamaños declarados dentro del archivo son datos, no verdad.**
   Un chunk que declara "tamaño = 4 GB" en un archivo de 2 KB debe
   producir `MalformedInput`, no un intento de lectura.
3. **Límite de recursos antes de asignar.** Antes de reservar memoria
   para N samples/presets/zonas declarados por el archivo, comprobar
   que N es plausible para el tamaño real del archivo (p.ej., N no
   puede ser mayor que el número de bytes restantes / tamaño mínimo de
   un registro) — si no, `ResourceLimitExceeded`.
4. **Nunca confiar en null-termination de strings dentro de chunks
   binarios** (los nombres SF2 tienen longitud fija definida por
   spec) — leer exactamente el número de bytes especificado por la
   spec, nunca hasta un terminador.
5. **`parse()` es `noexcept`**: cualquier condición de error debe
   devolverse como `Result::fail`, nunca como excepción — esto ya está
   fijado en el contrato actual.

## Qué NO se implementa todavía

Ninguna de estas reglas tiene código real todavía — `Sf2Parser` sigue
siendo una interfaz abstracta sin cuerpo. Este documento existe para
que la primera implementación (Fase B) no tenga que redescubrir estas
reglas ni retrasar su aplicación a "después".

## Fase B — verificación real de estas reglas

`native/soundfont/src/sf2_parser_impl.cpp` implementa las 5 reglas de
arriba (bounds checking vía `detail::SafeByteReader`, tamaños
declarados nunca confiados sin validar, `checkedMul` para detectar
overflow antes de calcular tamaños de registro, lectura de nombres de
longitud fija sin asumir null-termination, `parse()` sigue siendo
`noexcept` devolviendo `Result::fail` en cada caso de dato inválido).

Verificado de forma real, no solo por diseño:
- **AddressSanitizer + UndefinedBehaviorSanitizer**: round-trip
  completo y los 3 fixtures malformados (`not_riff.sf2`,
  `truncated_pdta.sf2`, `bad_riff_size.sf2`) pasan limpio, sin ningún
  reporte de lectura fuera de rango ni UB.
- **Fuzz smoke test** (`tests/soundfont/fuzz_smoke_test.cpp`): 2000
  buffers puramente aleatorios + 2000 mutaciones del fixture válido
  (bytes al azar + truncamientos al azar), bajo los mismos
  sanitizers — 0 crashes en las 4000 entradas.

Esto no sustituye un fuzzer real de cobertura guiada (libFuzzer/AFL)
para un endurecimiento de seguridad exhaustivo, pero da evidencia
concreta de que las reglas de esta página se cumplen en la
implementación real, no solo en la intención de diseño.
