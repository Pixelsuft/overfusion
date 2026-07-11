#pragma once
#include <cstdint>

namespace event {
enum class Type : uint8_t {
    None = 0,
    KeyDown = 1,
    MouseDown = 2,
    MouseMove = 3,
    HashCheck = 4, // TODO: move this event type to own struct?
    SetSeed = 5,
    PushRandom = 6,
    PopRandom = 7,
    MsgBox = 20
};

// Tagged union, in short
struct Event {
    int frame;
    union {
        // Keyboard or mouse down/up event
        struct {
            short k;
            bool down;
        } key;
        // Mouse move event
        struct {
            float x;
            float y;
        } mouse;
        // Message box event
        struct {
            int choice;
        } msgbox;
        // RNG event
        struct {
            uint16_t range;
            uint16_t value;
            uint16_t repeat;
        } rng;
        uint64_t dummy;
    };
    Type idx;

    Event() : dummy(0), frame(0), idx(Type::None) {}
};
} // namespace event
