#include "state.hpp"
#include "ass.hpp"
#include "ofs.hpp"
#include "plugbase.hpp"
#include <spdlog/spdlog.h>

static bool success = true;

class State {
public:
    uint64_t system_offset;
    uint64_t local_offset;
    uint64_t startup_offset;
    int frames;

    State() { clear_values(); }

    void clear_values() {
        system_offset = local_offset = startup_offset = 0;
        frames = 0;
    }
};

static State st;

void state::init() {}

void state::invalidate_process() { success = false; }

void state::after_update() {
    st.frames++;
}

int64_t state::get_utc_offset() {
    return static_cast<int64_t>(st.local_offset) - static_cast<int64_t>(st.system_offset);
}

uint64_t state::get_time(TimeOffset offset) {
    auto fps = plug::get().fps;
    uint64_t ret = static_cast<uint64_t>(st.frames) * 1000 / fps;
    switch (offset) {
    case TimeOffset::None:
        return ret;
    case TimeOffset::System:
        return ret + st.system_offset;
    case TimeOffset::Local:
        return ret + st.local_offset;
    case TimeOffset::Startup:
        return ret + st.startup_offset;
    case TimeOffset::Reminder:
        return static_cast<uint64_t>(st.frames) * 1000 % fps;
    default:
        __assume(false);
        return 0;
    }
}

bool state::save(ofs::File& file) {
    ASS(file.is_open());
    success = true;
    return plug::get().save_state(file) && success;
}

bool state::load(ofs::File& file) {
    ASS(file.is_open());
    success = true;
    return plug::get().load_state(file) && success;
}
