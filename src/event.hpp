#pragma once
#include <cstdint>

namespace event {
struct Event {
    int frame;
    union {
        struct {
            short k;
            bool down;
        } key;
        struct {
            float x;
            float y;
        } mouse;
        struct {
            short x;
            short y;
            short z;
            short w;
        } dummy;
    };
    uint8_t idx;

    Event() : dummy(0, 0, 0, 0), frame(0), idx(0) {}
};
} // namespace event
