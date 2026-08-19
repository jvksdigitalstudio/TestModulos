# native/platform — única frontera hacia Oboe/AAudio/NDK.
#
# `olysf2sampler_platform` (INTERFACE): solo el contrato AudioBackend,
# header-only, compilable en cualquier host (sin Oboe). Es lo que
# consumen olysf2sampler_audio y cualquier código que no necesite la
# implementación concreta.
#
# `olysf2sampler_platform_android` (STATIC, SOLO cuando ANDROID=ON):
# la implementación real respaldada por Oboe. Descarga Oboe vía
# FetchContent — requiere red, por eso solo se intenta bajo Android
# (el build de host/tests nunca lo necesita). Esta es la ÚNICA parte
# de todo el proyecto que no se compiló en el entorno de desarrollo
# (sin red ahí) — ver oboe_audio_backend.cpp para la advertencia
# completa y docs/architecture/AUDIO_ENGINE.md.

add_library(olysf2sampler_platform INTERFACE)

target_include_directories(olysf2sampler_platform
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(olysf2sampler_platform INTERFACE olysf2sampler_core)
target_compile_features(olysf2sampler_platform INTERFACE cxx_std_20)

if(ANDROID)
    include(FetchContent)
    FetchContent_Declare(
        oboe
        GIT_REPOSITORY https://github.com/google/oboe.git
        GIT_TAG        1.9.3  # versión estable pinneada explícitamente
    )
    FetchContent_MakeAvailable(oboe)

    add_library(olysf2sampler_platform_android STATIC
        src/oboe_audio_backend.cpp
    )

    target_include_directories(olysf2sampler_platform_android
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(olysf2sampler_platform_android
        PUBLIC
            olysf2sampler_core
            oboe
    )
    target_compile_features(olysf2sampler_platform_android PUBLIC cxx_std_20)
endif()
