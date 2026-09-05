plugins {
    `kotlin-dsl`
}

repositories {
    google()
    mavenCentral()
    gradlePluginPortal()
}

dependencies {
    compileOnly("com.android.tools.build:gradle:8.7.0")
    compileOnly("org.jetbrains.kotlin:kotlin-gradle-plugin:2.0.21")
}

gradlePlugin {
    plugins {
        register("olyzeAndroidLibrary") {
            id = "olyze.android.library"
            implementationClass = "com.yeivikas.buildlogic.OlyzeAndroidLibraryConventionPlugin"
        }
        register("olyzeKotlinJvm") {
            id = "olyze.kotlin.jvm"
            implementationClass = "com.yeivikas.buildlogic.OlyzeKotlinJvmConventionPlugin"
        }
    }
}
