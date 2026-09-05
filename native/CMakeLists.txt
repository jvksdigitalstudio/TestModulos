# native/ — CMakeLists.txt raíz del núcleo C++ de OlySf2 Sampler.
#
# Este árbol es intencionalmente independiente del namespace/identidad
# "EliNer": EliNer se está implementando en el proyecto de Olyze Music
# Studio. Este módulo (OlySf2 Sampler) NO reimplementa EliNer ni crea
# un motor paralelo — implementa su propio núcleo de sampler/SoundFont,
# diseñado para integrarse posteriormente DETRÁS de la EliNer API real
# cuando ese proyecto lo consuma (ver docs/decisions/ADR-002).
#
# Este archivo SOLO agrega subdirectorios y define el orden de
# dependencia entre módulos — nada de lógica mezclada aquí. Cada
# módulo define su propio target en su propio CMakeLists.txt.
#
# Orden de dependencia (de base a fachada):
#   core -> dsp, memory, threading, diagnostics, device, resources, midi, samples, soundfont
#        -> scheduler (usa threading)
#        -> platform (única frontera Android/Oboe real)
#        -> audio (usa platform)
#        -> sampler (usa dsp)
#   jni  -> agrega todo lo anterior como una única libolysf2sampler.so

cmake_minimum_required(VERSION 3.22.1)
project(olysf2sampler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fase A.1 §23 — FIX: ninguna biblioteca de este árbol se compilaba
# con -fPIC/POSITION_INDEPENDENT_CODE, pese a que TODAS son STATIC
# (ver comentarios "STATIC" en cada CMakeLists.txt de módulo) y
# terminan enlazadas dentro de bibliotecas SHARED reales:
#   jni/CMakeLists.txt          -> add_library(olysf2sampler SHARED ...)
#   host_adapter/CMakeLists.txt -> add_library(olysf2sampler_module SHARED ...)
# Enlazar código objeto no-PIC dentro de una biblioteca compartida es
# el escenario exacto que §23 pide corregir ("static native libraries
# -> shared host adapter -> -fPIC"): en muchos toolchains (en
# particular Android NDK/clang para arm64-v8a/armeabi-v7a, el target
# real de este proyecto) esto falla en el link con reubicaciones de
# texto, o produce una .so con reubicaciones de texto que impiden
# ASLR correctamente. En este entorno de verificación (Ubuntu 24.04 /
# GCC 13 configurado con --enable-default-pie) NO pude reproducir el
# fallo de link de forma concluyente con un repro mínimo -- el
# default-pie de este GCC concreto enmascara el problema para código
# trivial -- así que esto se corrige defensivamente como buena
# práctica estándar de CMake (costo cero, sin downside), no porque se
# haya confirmado el fallo exacto contra el NDK real (que no está
# disponible en este entorno; ver §24 del build). Se aplica UNA vez
# aquí: CMAKE_POSITION_INDEPENDENT_CODE es respetado automáticamente
# por CMake para todo target definido en este directorio y en
# cualquier add_subdirectory() bajo él, sin tocar cada
# CMakeLists.txt de módulo individualmente.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Módulos base sin dependencias entre sí (aparte de olysf2sampler_core)
add_subdirectory(core)
add_subdirectory(memory)
add_subdirectory(threading)
add_subdirectory(diagnostics)
add_subdirectory(device)
add_subdirectory(resources)
add_subdirectory(midi)
add_subdirectory(samples)
add_subdirectory(soundfont)
add_subdirectory(dsp)

# Módulos que dependen de los anteriores
add_subdirectory(scheduler)   # depende de threading
add_subdirectory(platform)    # única frontera Android/Oboe
add_subdirectory(audio)       # depende de platform
add_subdirectory(sampler)     # depende de dsp

# Target agregador: agrupa todos los módulos de OlySf2 Sampler en una
# sola interfaz de link para quien consuma el núcleo completo
# (típicamente jni/). Ningún módulo de arriba depende de este target.
add_library(olysf2sampler_all INTERFACE)
target_link_libraries(olysf2sampler_all INTERFACE
    olysf2sampler_core
    olysf2sampler_memory
    olysf2sampler_threading
    olysf2sampler_diagnostics
    olysf2sampler_device
    olysf2sampler_resources
    olysf2sampler_midi
    olysf2sampler_samples
    olysf2sampler_soundfont
    olysf2sampler_dsp
    olysf2sampler_scheduler
    olysf2sampler_platform
    olysf2sampler_audio
    olysf2sampler_sampler
)

# Los tests nativos (tests/integration) se registran desde su propio
# CMakeLists.txt, que hace add_subdirectory hacia aquí — para evitar
# que este archivo mezcle configuración de producción con la de test.
