#pragma once
// native/midi — modelo de eventos MIDI y gestión de
// entrada/salida. Responsabilidad única: representar y despachar
// eventos MIDI hacia el sampler; no decide polifonía ni mapeo.

#include <cstdint>

namespace olysf2sampler::midi {

enum class MidiEventType {
    NoteOn,
    NoteOff,
    ControlChange,
    PitchBend,
};

struct MidiEvent {
    MidiEventType type;
    std::uint8_t channel{0};
    std::uint8_t data1{0};  // note / controller
    std::uint8_t data2{0};  // velocity / value
};

}  // namespace olysf2sampler::midi
