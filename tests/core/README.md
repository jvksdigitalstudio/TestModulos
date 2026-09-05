# tests/core

Reservado para tests unitarios de `native/core` cuando ese subsistema
reciba implementación real (más allá de los contratos actuales).

**Estado: vacío intencionalmente** — no se crean tests placeholder
sin código real que probar (regla de no fingir resultados). El único
test existente hoy que ejercita este módulo es el smoke test cruzado
en `tests/integration/`, que confirma que `native/core` compila y
enlaza junto al resto del núcleo.
