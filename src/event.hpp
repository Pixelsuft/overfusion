#pragma once
#include <cstdint>

namespace event {
enum class Type : uint8_t { None = 0, KeyDown = 1, MouseDown = 2, MouseMove = 3 };

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
        uint64_t dummy;
    };
    Type idx;

    Event() : dummy(0), frame(0), idx(Type::None) {}
};
} // namespace event
