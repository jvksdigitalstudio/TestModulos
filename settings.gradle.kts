pluginManagement {
    includeBuild("build-logic")
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "olysf2-sampler"

// OlySf2 Sampler NO es una aplicación independiente: es un módulo
// Synth/Sampler reutilizable (Android library) diseñado para
// integrarse en Olyze Music Studio a través de EliNer. Por eso este
// repo no declara ningún módulo `:app`.
//
// `:api` es la única frontera Kotlin del módulo — el resto del núcleo
// (native/, jni/) no son módulos Gradle Kotlin/Android propios, sino
// el árbol fuente C++ referenciado por externalNativeBuild desde
// `api/build.gradle.kts` (ver ese archivo).
include(":api")
