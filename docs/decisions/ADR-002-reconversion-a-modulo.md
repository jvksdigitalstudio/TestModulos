# ADR-002 — Reconversión de aplicación independiente a módulo OlySf2 Sampler

## Estado
Aceptado.

## Contexto

El repositorio nació como scaffolding de "Olyze Sf2 Creator", una
aplicación Android independiente, con un motor propio llamado EliNer
implementado dentro del mismo repositorio (`eliner/api`,
`eliner/native`, `eliner/jni`).

La estrategia de producto cambió: "Olyze Sf2 Creator" como aplicación
independiente deja de existir. En su lugar:

1. Se está desarrollando **Olyze Music Studio**, un DAW Android
   profesional, en otro proyecto.
2. **EliNer** (el motor/API reutilizable) se está implementando dentro
   de ese proyecto de Olyze Music Studio, no aquí.
3. Este repositorio pasa a construir **OlySf2 Sampler**: un módulo
   Synth/Sampler profesional reutilizable, sin UI de aplicación propia,
   que se integrará como instrumento dentro de Olyze Music Studio a
   través de la EliNer API real cuando ese proyecto lo consuma.

## Decisión

1. **Se elimina por completo el módulo `app/`** (Activity, launcher,
   Application ID `com.yeivikas.olyzesf2c`, navegación Compose,
   ViewModels, preferencias, y el formato de proyecto `.olysf2c` con
   su model/serialization/storage). Ninguno de estos conceptos
   pertenece a un módulo reutilizable.

2. **Se retira el namespace/identidad "EliNer" de este repositorio.**
   El código que antes vivía bajo `eliner/native` y `eliner/api` (namespace
   C++ `eliner::`, paquete Kotlin `com.yeivikas.eliner`) se renombra a
   `native/` y `api/` respectivamente, con namespace `olysf2sampler::`
   y paquete `com.yeivikas.olysf2sampler`. Razón: mantener el nombre
   "EliNer" aquí, cuando EliNer ya se está construyendo como proyecto
   separado, habría creado exactamente lo que el encargo prohíbe
   explícitamente — una segunda arquitectura EliNer paralela y
   duplicada. El código en sí (Result<T>, contratos de soundfont/
   sampler/dsp/audio/etc.) no se descarta: es una base técnica sólida
   y coincide con la arquitectura pedida para OlySf2 Sampler; solo deja
   de llamarse ni comportarse como si fuera EliNer.

3. **Se mantiene la dirección de dependencia unidireccional**: OlySf2
   Sampler no depende de Olyze Music Studio ni de EliNer; en el
   futuro, EliNer (dentro de Olyze Music Studio) dependerá de OlySf2
   Sampler, nunca al revés.

4. `.sf2` sigue siendo el único formato de archivo relevante para este
   módulo (importación/edición/exportación SoundFont). No se inventa
   ningún formato de proyecto nuevo en su lugar.

## Alternativas consideradas

- **Renombrar `eliner` → `olysf2sampler` solo en el texto, pero
  mantener el mismo repositorio como "el EliNer real":** rechazada.
  El encargo es explícito en que EliNer ya se implementa en otro
  proyecto; duplicarlo aquí (aunque fuera con otro nombre superficial)
  generaría dos fuentes de verdad para el mismo motor.
- **Mantener `app/` como "app de desarrollo/demo" para probar el
  módulo manualmente:** rechazada por ahora. El encargo prohíbe
  explícitamente crear una segunda aplicación Android o mantener
  lógica de aplicación independiente. Si en el futuro se necesita un
  harness de prueba manual, debe ser un target de test explícito
  (`androidTest` con una Activity mínima de solo-test), no una
  aplicación de producto con su propio Application ID.

## Consecuencias

- Positivas: el módulo puede compilarse y probarse en host sin ningún
  concepto de Android UI (confirmado — ver informe de reconversión,
  compilación real con g++). Queda listo para integrarse en Olyze
  Music Studio sin necesitar "desmontar" una aplicación.
- Negativas / costo: se pierde la capacidad de ejecutar la app en un
  dispositivo/emulador para probar manualmente hasta que exista un
  harness de test dedicado o hasta la integración real en Olyze Music
  Studio.

## Verificación de la regla

"¿Sigue este repositorio pareciendo, en cualquier sentido, una
aplicación Android independiente?" — No: no hay Application ID, no hay
Activity, no hay launcher, no hay `.olysf2c`. Confirmado por auditoría
de archivos en el informe de reconversión (Fase A).
