# tests/integration

`smoke_test.cpp` — smoke test cruzado: incluye los headers públicos de
los 14 módulos de `native/` y ejercita `Result<T>` y un par de tipos de
datos simples. Confirma que el núcleo compila y enlaza como un todo,
sin dependencias circulares.

No es un test funcional del sampler (todavía no hay sampler real que
probar) ni de round-trip SF2 (todavía no hay parser/writer real).
Cuando existan, sus tests de integración de más alto nivel (p.ej.
"cargar SF2 → reproducir nota → verificar buffer de salida") vivirán
también aquí.
