# ADR-001 — EliNer como motor reutilizable independiente de producto

## Estado
**Superado por ADR-002.** Se conserva como registro histórico: el
razonamiento sobre "el motor no debe depender del producto consumidor"
sigue siendo válido y es precisamente la base de ADR-002, pero el
contexto de producto cambió por completo (ya no existe "Olyze Sf2
Creator" como aplicación, y EliNer ya no se implementa en este
repositorio — ver ADR-002).

## Contexto
Olyze Sf2 Creator es el primer producto de YeiViKas Digital Company
que necesita un motor de audio/SoundFont/sampler. Existe un segundo
producto planeado, Olyze Music Studio, que necesitará capacidades
equivalentes (audio de baja latencia, sampler, posiblemente SoundFont).

## Decisión
EliNer se diseña y ubica en el repositorio (`eliner/`) como módulo
independiente de `app/` desde el primer commit. `app/` depende de
`eliner/api`; `eliner/` nunca depende de `app/`. Esta dirección de
dependencia se hace cumplir estructuralmente (módulos Gradle
separados, CMakeLists separado) y debe hacerse cumplir en CI cuando
exista pipeline de build real.

```
Olyze Sf2 Creator → EliNer
Olyze Music Studio → EliNer   (futuro, sin duplicar código)
```

## Alternativas consideradas
- **Construir el motor dentro de `app/` y extraerlo después:**
  rechazada explícitamente por el usuario (Fase 1 §4) — la experiencia
  típica es que "extraer después" nunca ocurre limpiamente y el motor
  termina con acoplamientos a Compose/Android lifecycle difíciles de
  revertir.

## Consecuencias
- Positivas: Olyze Music Studio podrá consumir EliNer sin copiar
  código, siempre que respete la Public API versionada.
- Negativas / costo: Fase 2 requiere más andamiaje (dos módulos
  Gradle, CMake separado, frontera JNI explícita) que si todo viviera
  en un único módulo `app`. Se acepta el costo por la razón anterior.

## Verificación de la regla
"¿Podríamos eliminar `app/` completamente y seguir teniendo un EliNer
compilable?" — Sí: `eliner/native` compila de forma independiente (ver
informe de Fase 2, verificación con g++). `eliner/api` no fue
verificable en este entorno por falta de Gradle/Kotlin, pero no
declara ninguna dependencia hacia `app/` en su `build.gradle.kts`.
