# NAMING_CONVENTIONS.md

- Nombres explícitos de lo que la clase administra: `VoiceManager`
  (administra voces), nunca `Manager`/`Helper`/`Utils` sin calificar.
- Prohibido: `Temp`, `Misc`, `Stuff`, `Test2`, `NewEngine`,
  `FinalEngine`, `OldEngine` — y prohibido también reintroducir
  `Olyze Sf2 Creator` / `.olysf2c` / `com.yeivikas.olyzesf2c` (identidad
  retirada, ver ADR-002).
- C++: `PascalCase` para tipos, `camelCase` para funciones/métodos,
  `snake_case` para archivos (`voice_manager.hpp`). Namespace raíz:
  `olysf2sampler::` (nunca `eliner::` — ese namespace pertenece al
  proyecto de Olyze Music Studio).
- Kotlin: paquete raíz `com.yeivikas.olysf2sampler` (nunca
  `com.yeivikas.eliner`, que pertenece al otro proyecto).
- `PascalCase` para tipos, `camelCase` para funciones/propiedades,
  archivo con el mismo nombre que la clase pública principal.
