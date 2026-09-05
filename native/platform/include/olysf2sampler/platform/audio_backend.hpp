#pragma once
// native/platform — ÚNICO punto de OlySf2 Sampler que debe conocer
// detalles de la plataforma (Android NDK, Oboe/AAudio). El resto del
// módulo (audio/, sampler/, etc.) programa contra esta abstracción,
// nunca contra Oboe/AAudio directamente.
//
// Fase D: la implementación real (OboeAudioBackend, en
// src/oboe_audio_backend.cpp) SOLO se compila cuando el build target
// es Android (ver native/platform/CMakeLists.txt, guardado con
// `if(ANDROID)` + CMake FetchContent de Oboe). Este entorno de
// desarrollo no tiene acceso de red para descargar Oboe, así que esa
// implementación concreta NO se ha podido compilar ni probar
// localmente — es la única pieza de todo el proyecto en esa
// situación. Se verifica por primera vez en CI (GitHub Actions sí
// tiene red). Ver docs/architecture/AUDIO_ENGINE.md para el detalle
// completo de qué se verificó y qué no.

#include <cstddef>
#include <functional>
#include <memory>

namespace olysf2sampler::platform {

struct AudioBackendConfig {
    int sampleRate{48000};
    int framesPerBuffer{0};  // 0 = usar el óptimo reportado por la plataforma
    int channelCount{2};
};

using AudioRenderCallback = std::function<void(float* outputBuffer, std::size_t frameCount)>;

/// Abstracción sobre el backend de audio real de la plataforma
/// (pensado para ser implementado con Oboe, con fallback interno de
/// Oboe a AAudio/OpenSL ES). Ningún otro módulo de OlySf2 Sampler debe incluir
/// headers de Oboe: todos pasan por esta interfaz.
class AudioBackend {
public:
    virtual ~AudioBackend() = default;

    virtual bool open(const AudioBackendConfig& config) noexcept = 0;
    virtual void close() noexcept = 0;

    virtual void setRenderCallback(AudioRenderCallback callback) = 0;

    virtual bool start() noexcept = 0;
    virtual void stop() noexcept = 0;

    virtual double reportedLatencyMillis() const noexcept = 0;
};

/// Construye el backend real respaldado por Oboe. SOLO existe una
/// definición de esta función cuando se compila para Android
/// (`#ifdef __ANDROID__` en oboe_audio_backend.cpp); en un build de
/// host, esta declaración existe pero nada la enlaza — es responsable
/// de quien ensambla el motor completo (jni/) usarla solo en el
/// target Android real.
std::unique_ptr<AudioBackend> createOboeAudioBackend();

}  // namespace olysf2sampler::platform
