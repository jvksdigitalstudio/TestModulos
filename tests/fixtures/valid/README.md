SF2 válidos conforme a especificación, para tests de round-trip.

## minimal.sf2 (638 bytes)

Generado con `tools/generate_minimal_sf2_fixture.cpp` (usa el
`Sf2Writer` real del proyecto, no bytes hechos a mano). Contiene: 1
sample (8 frames PCM de prueba con loop points), 1 instrumento con 1
zona que referencia ese sample, 1 preset con 1 zona que referencia ese
instrumento, y metadata INFO completa (isng/INAM/IPRD/ICOP/ICMT/ISFT).

Para regenerarlo tras cambiar el writer:
```
g++ -std=c++20 -I native/core/include -I native/soundfont/include \
    native/core/src/version.cpp native/soundfont/src/sf2_writer_impl.cpp \
    tools/generate_minimal_sf2_fixture.cpp -o /tmp/gen_fixture
/tmp/gen_fixture   # ejecutar desde la raíz del repo
```

Licencia: generado por este proyecto, no contiene audio de terceros —
sin restricciones de licencia.
