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
            short x;
            short y;
            short z;
            short w;
        } dummy;
    };
    uint8_t idx;

    Event() {
        dummy.x = dummy.y = dummy.z = dummy.w = 0;
        frame = 0;
        idx = 0;
    }
};
} // namespace event
