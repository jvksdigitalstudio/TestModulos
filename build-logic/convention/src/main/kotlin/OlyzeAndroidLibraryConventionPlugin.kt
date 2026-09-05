package com.yeivikas.buildlogic

import com.android.build.api.dsl.LibraryExtension
import org.gradle.api.Plugin
import org.gradle.api.Project
import org.gradle.kotlin.dsl.configure

/**
 * Configuración compartida para módulos de librería Android.
 * Usada por `api/` (la Public API de OlySf2 Sampler), que empaqueta
 * la librería nativa vía externalNativeBuild cuando corresponda.
 * OlySf2 Sampler no tiene módulo de aplicación: es un módulo
 * reutilizable, no un launcher.
 */
class OlyzeAndroidLibraryConventionPlugin : Plugin<Project> {
    override fun apply(target: Project) {
        with(target) {
            pluginManager.apply("com.android.library")
            pluginManager.apply("org.jetbrains.kotlin.android")

            extensions.configure<LibraryExtension> {
                compileSdk = 35

                defaultConfig {
                    minSdk = 26
                }

                compileOptions {
                    sourceCompatibility = org.gradle.api.JavaVersion.VERSION_17
                    targetCompatibility = org.gradle.api.JavaVersion.VERSION_17
                }
            }
        }
    }
}
