package com.yeivikas.buildlogic

import org.gradle.api.Plugin
import org.gradle.api.Project

/**
 * Reservado para módulos Kotlin puros (JVM) sin dependencia de Android,
 * p.ej. utilidades de `tools/` o futuros módulos de dominio compartido
 * que no necesiten el Android Gradle Plugin.
 */
class OlyzeKotlinJvmConventionPlugin : Plugin<Project> {
    override fun apply(target: Project) {
        target.pluginManager.apply("org.jetbrains.kotlin.jvm")
    }
}
