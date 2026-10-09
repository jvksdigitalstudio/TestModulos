# API.md — Contratos de OlySf2 Sampler

Índice; el código fuente es la fuente de verdad.

## Kotlin (api/)

- `com.yeivikas.olysf2sampler.OlySf2SamplerEngine` — ciclo de vida del núcleo.
- `com.yeivikas.olysf2sampler.AudioSessionService` — sesión de audio.
- `com.yeivikas.olysf2sampler.SamplerService` — reproducción/mapeo.
- `com.yeivikas.olysf2sampler.SoundFontService` — parse/write/validate.
- `com.yeivikas.olysf2sampler.EngineDiagnostics` — snapshot de estado.

Estos contratos están escritos pensando en que, cuando este módulo se
integre en Olyze Music Studio, EliNer pueda consumirlos directamente o
adaptarlos con una capa fina — de ahí que se mantengan deliberadamente
mínimos (§16/§32 del encargo de reconversión: no sobrediseñar antes de
tener implementación real).

## C++ (native/)

Headers públicos por módulo bajo `native/<módulo>/include/olysf2sampler/<módulo>/`.
Ejemplos: `olysf2sampler/soundfont/parser.hpp` (`Sf2Parser`),
`olysf2sampler/sampler/sampler_engine.hpp` (`SamplerEngine`),
`olysf2sampler/platform/audio_backend.hpp` (`AudioBackend`).

Todos son interfaces abstractas sin implementación concreta en esta
fase.
