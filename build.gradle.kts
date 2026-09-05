// Root build file — OlySf2 Sampler (YeiViKas Digital Company).
// Módulo Synth/Sampler reutilizable, NO aplicación independiente:
// por eso este archivo no declara el plugin "com.android.application".
// No se declara lógica de build aquí: toda la configuración compartida
// vive en `build-logic/` como convention plugins.

plugins {
    id("com.android.library") version "8.7.0" apply false
    id("org.jetbrains.kotlin.android") version "2.0.21" apply false
    id("org.jetbrains.kotlin.jvm") version "2.0.21" apply false
}

tasks.register("architectureAudit") {
    group = "verification"
    description = "Recuerda las reglas de dependencia: native/ y api/ nunca dependen de un consumidor (ni Olyze Music Studio, ni EliNer)."
    doLast {
        println("Auditoría manual recomendada: ver docs/decisions/ADR-002-reconversion-a-modulo.md")
    }
}
