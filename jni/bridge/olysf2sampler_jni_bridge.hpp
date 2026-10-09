#pragma once
// jni/bridge — ÚNICA frontera JNI de OlySf2 Sampler.
//
// Reglas estrictas de este archivo (y de todo jni/):
//   - Solo creación/destrucción del núcleo, comunicación controlada y
//     conversión de tipos.
//   - CERO lógica de DSP, parsing SF2, sampler o procesamiento de
//     samples aquí: eso vive en native/ y se invoca desde acá.
//   - Nada de este archivo se ejecuta dentro del audio callback.

#include <jni.h>

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_yeivikas_olysf2sampler_internal_NativeOlySf2SamplerEngine_nativeInitialize(
    JNIEnv* env, jobject thiz, jint preferredSampleRate);

JNIEXPORT void JNICALL
Java_com_yeivikas_olysf2sampler_internal_NativeOlySf2SamplerEngine_nativeShutdown(
    JNIEnv* env, jobject thiz);

// Fase D: superficie mínima para demostrar el pipeline de audio real
// de extremo a extremo (JNI -> SamplerEngine -> AudioEngine -> Oboe).
// Dispara/suelta una nota sobre el tono de prueba cargado en
// nativeInitialize. La carga real de SoundFonts (.sf2) y una API de
// notas completa son alcance de una fase posterior — ver
// docs/architecture/AUDIO_ENGINE.md.
JNIEXPORT void JNICALL
Java_com_yeivikas_olysf2sampler_internal_NativeOlySf2SamplerEngine_nativeNoteOn(
    JNIEnv* env, jobject thiz, jint midiNote, jint velocity);

JNIEXPORT void JNICALL
Java_com_yeivikas_olysf2sampler_internal_NativeOlySf2SamplerEngine_nativeNoteOff(
    JNIEnv* env, jobject thiz, jint midiNote);

}  // extern "C"
