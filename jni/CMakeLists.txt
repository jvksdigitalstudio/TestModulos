# jni/ — frontera JNI mínima de OlySf2 Sampler.
# Requiere el NDK (jni.h) para compilar; por eso es un target separado
# de native/, que es 100% host-buildable sin NDK. Este target se
# construye como parte del build Android real (AGP + NDK) —
# ver api/build.gradle.kts (externalNativeBuild apunta aquí) y
# .github/workflows/build.yml para la compilación real en CI.

cmake_minimum_required(VERSION 3.22.1)
project(olysf2sampler_jni LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../native ${CMAKE_CURRENT_BINARY_DIR}/olysf2sampler_native)

add_library(olysf2sampler SHARED
    bridge/olysf2sampler_jni_bridge.cpp
)

# olysf2sampler_all trae el contrato de todos los módulos;
# olysf2sampler_platform_android trae la implementación real de Oboe
# (definida en native/platform/CMakeLists.txt, solo existe cuando
# ANDROID=ON — que siempre es el caso al construir este target, ya
# que jni/ solo tiene sentido dentro de un build Android real).
target_link_libraries(olysf2sampler PRIVATE olysf2sampler_all olysf2sampler_platform_android)

find_library(log-lib log)
target_link_libraries(olysf2sampler PRIVATE ${log-lib})
