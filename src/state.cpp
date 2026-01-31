#include "state.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "ofs.hpp"
#include "plugbase.hpp"
#include <spdlog/spdlog.h>
#include <vector>

static bool success = true;

struct Event {
    int frame;

    Event() : frame(0) {}
};

class State {
public:
    std::vector<Event> ev;
    std::vector<Event> temp_ev;
    std::vector<int> prev;
    uint64_t system_offset;
    uint64_t local_offset;
    uint64_t startup_offset;
    int scene;
    int fps;
    int frames;
    int total;

    State() { clear_values(); }

    void clear_values() {
        system_offset = local_offset = startup_offset = 0;
        scene = 0;
        frames = total = 0;
        fps = 0;
    }

    void clear_lists() {
        ev.clear();
        temp_ev.clear();
        prev.clear();
    }
};

static State st;

void state::init() {
    st.fps = conf::get().fps;
    spdlog::info("Init FPS: {}", st.fps);
}

void state::invalidate_process() { success = false; }

void state::after_update() { st.frames++; }

int64_t state::get_utc_offset() {
    return static_cast<int64_t>(st.local_offset) - static_cast<int64_t>(st.system_offset);
}

uint64_t state::get_time(TimeOffset offset) {
    auto fps = st.fps;
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
