# MEMORY_AND_OWNERSHIP.md

Auditoría de propiedad de memoria pedida en §22 del encargo de Fase A.

## Regla general

- **RAII por defecto** para todo lo que no esté en el hot path del
  audio callback: `std::unique_ptr` para propiedad exclusiva,
  `std::vector`/`std::string` para buffers/datos owned normales.
- **`std::shared_ptr` NO es el default.** Solo se justifica cuando
  varios subsistemas necesitan genuinamente co-poseer el mismo objeto
  con tiempo de vida indeterminado (p.ej., un `SoundFontModel` cargado
  que varias voces podrían referenciar mientras el usuario edita en
  paralelo — a decidir caso por caso en la fase de implementación, no
  por defecto).
- **Raw pointers nunca representan ownership.** Cuando aparecen (como
  en `BufferPool::acquire() -> float*`), son referencias no
  propietarias a memoria cuyo dueño real es el pool. El método
  `release()` existe precisamente porque el buffer no es del llamador.

## Caso específico: `memory::BufferPool`

`float* acquire()` devuelve un puntero no propietario a un buffer
pre-asignado. El contrato deja explícito (ver `buffer_pool.hpp`) que:
- el pool posee la memoria real (asignada en `preallocate`);
- el llamador debe devolverla con `release()`;
- ninguna de las dos operaciones aloja/libera memoria del sistema en
  el hot path — solo mueve punteros dentro de una estructura
  pre-reservada.

Esto es intencional: `shared_ptr` con su contador atómico introduciría
overhead impredecible en el audio callback (Fase 1 §13); un pool con
ownership centralizado y punteros no propietarios evita esa
imprevisibilidad sin recurrir a raw pointers como propiedad.

## Caso específico: `soundfont::SoundFontModel`

Es un value type (structs con `std::vector`/`std::string` — ownership
por valor estándar de C++, sin punteros). El parser lo construye y lo
devuelve por valor (via `Result<SoundFontModel>`); no hay ownership
compartido en el modelo en memoria hasta que exista una razón real
para introducirlo.

## Pendiente

Esta es una política, no una implementación: se re-audita cuando cada
subsistema reciba código real (Fase B en adelante), en particular
`sampler::Voice` (¿quién posee el buffer de sample que reproduce una
voz? probablemente una referencia no propietaria a un `SoundFontModel`
cargado, nunca una copia) y `threading::WorkerThread` (paso de datos
entre hilos: por valor/move, no por puntero compartido, salvo que se
demuestre necesario).
