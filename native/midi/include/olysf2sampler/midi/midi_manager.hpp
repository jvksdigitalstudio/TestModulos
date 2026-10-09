#pragma once
// Responsabilidad única de MidiManager: recibir eventos MIDI de la
// plataforma (vía olysf2sampler::platform) y despacharlos como MidiEvent
// hacia quien esté suscrito (típicamente olysf2sampler::sampler::SamplerEngine,
// conectado por la capa que ensambla el motor, no por midi mismo).

#include <functional>

#include "olysf2sampler/midi/midi_event.hpp"

namespace olysf2sampler::midi {

using MidiEventCallback = std::function<void(const MidiEvent&)>;

class MidiManager {
public:
    virtual ~MidiManager() = default;

    virtual void setEventCallback(MidiEventCallback callback) = 0;
    virtual void start() noexcept = 0;
    virtual void stop() noexcept = 0;
};

}  // namespace olysf2sampler::midi
