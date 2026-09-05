# third_party

Ninguna dependencia de terceros está integrada todavía.

Siguiendo la regla de Fase 1 §14, cada dependencia propuesta debe
evaluarse contra estos 11 criterios antes de agregarse aquí:

1. qué problema resuelve
2. si realmente es necesaria
3. licencia
4. mantenimiento
5. compatibilidad Android
6. rendimiento
7. tamaño
8. ABI
9. dependencia transitiva
10. posibilidad de reemplazarla por código propio
11. impacto futuro sobre OlySf2 Sampler

## Integrada: Oboe (Google)

- **Problema que resuelve:** abstrae AAudio con fallback automático a
  OpenSL ES/AudioTrack en dispositivos donde AAudio no está disponible
  o es inestable, evitando reimplementar esa lógica de compatibilidad.
- **Necesidad:** alta — es la razón declarada en Fase 1 §5 para no
  usar AAudio/AudioTrack directamente.
- **Licencia:** Apache 2.0 (compatible).
- **Mantenimiento:** activo (Google, uso en producción por múltiples
  apps de audio Android).
- **Compatibilidad Android:** diseñada específicamente para NDK.
- **Aislamiento:** se consume EXCLUSIVAMENTE desde
  `native/platform/src/oboe_audio_backend.cpp` (ver `audio_backend.hpp`).
  Ningún otro módulo incluye headers de Oboe — confirmado por grep.

**Estado (Fase D): integrada vía CMake `FetchContent`**, guardada tras
`if(ANDROID)` en `native/platform/CMakeLists.txt`, versión pinneada
explícitamente (`GIT_TAG 1.9.3`, no `main`/`master`, para build
reproducible). Se descarga y compila DENTRO del build de Android real
(AGP + NDK), nunca en el build de host/tests — los tests en este
repositorio nunca necesitan red.

**Advertencia honesta:** el entorno donde se escribió esta integración
no tenía acceso de red, así que `oboe_audio_backend.cpp` nunca se
compiló contra los headers reales de Oboe antes de entregarse — es la
única pieza de todo el proyecto en esa situación. Se verifica por
primera vez en CI. Ver `docs/architecture/AUDIO_ENGINE.md` para el
detalle completo.

## Regla permanente

No se agrega ninguna librería a este directorio sin la tabla de
evaluación de arriba documentada en un ADR bajo `docs/decisions/`.
