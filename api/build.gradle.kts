plugins {
    id("olyze.android.library")
}

android {
    namespace = "com.yeivikas.olysf2sampler"

    ndkVersion = "27.0.12077973"

    // Frontera con el núcleo nativo: apunta a jni/CMakeLists.txt (no a
    // native/CMakeLists.txt directamente), porque jni/ es quien agrega
    // native/ y produce la librería compartida real "libolysf2sampler.so"
    // que System.loadLibrary("olysf2sampler") espera cargar.
    externalNativeBuild {
        cmake {
            path = file("../jni/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    defaultConfig {
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters += listOf("arm64-v8a")
        }
        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++20"
                arguments += "-DANDROID_STL=c++_shared"
            }
        }
    }
}

dependencies {
    testImplementation("junit:junit:4.13.2")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test:runner:1.6.2")
}
