#include "state.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "ofs.hpp"
#include "plugbase.hpp"
#include "winhooks.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vector>

using std::string;

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
static std::vector<int> holding;
static std::vector<int> repl_holding;
static string last_msg;
static bool success;
static bool updating;
static bool need_key_msg;

void state::save_state(int slot) {
    ofs::File file(string("state_") + std::to_string(slot) + ".ostate", 1);
    if (!file.is_open()) {
        spdlog::warn("Failed to open state slot {} for writing", slot);
        return;
    }
    plug::get().save_state(file);
}

void state::load_state(int slot) {
    ofs::File file(string("state_") + std::to_string(slot) + ".ostate", 0);
    if (!file.is_open()) {
        spdlog::warn("Failed to open state slot {} for reading", slot);
        return;
    }
    plug::get().load_state(file);
}

void state::init() {
    need_key_msg = plug::get().get_bool_prop(plug::BoolProp::NeedKeyMsg);
    last_msg = "";
    success = false;
    updating = false;
    st.fps = conf::get().fps;
    spdlog::debug("Init FPS: {}", st.fps);
}

void state::invalidate_process() { success = false; }

void state::early_update() {}

void state::before_update() {
    updating = true;
    auto& cfg = conf::get();
    if (cfg.is_replay) {

    } else {
        for (auto it = holding.begin(); it != holding.end(); it++) {
            auto pit = std::find(st.prev.begin(), st.prev.end(), *it);
            if (pit == st.prev.end()) {
                // Down event
                if (need_key_msg)
                    winhooks::sim_key_event(*it, true);
            }
        }
        for (auto pit = st.prev.begin(); pit != st.prev.end(); pit++) {
            auto it = std::find(holding.begin(), holding.end(), *pit);
            if (it == holding.end()) {
                // Up event
                if (need_key_msg)
                    winhooks::sim_key_event(*pit, false);
            }
        }
    }
    st.prev = cfg.is_replay ? repl_holding : holding;
}

void state::after_update() {
    ASS(updating);
    updating = false;
    st.frames++;
    st.total = std::max(st.total, st.frames);
}

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

bool state::get_key_state(int vk) {
    auto& vec = updating ? (conf::get().is_replay ? repl_holding : holding) : st.prev;
    auto ret = std::find(vec.begin(), vec.end(), vk) != vec.end();
    return ret;
}

void state::set_key_down(int vk, bool down) {
    ASS(vk > 0 && vk < 256);
    auto it = std::find(holding.begin(), holding.end(), vk);
    if (down && it == holding.end())
        holding.push_back(vk);
    else if (!down && it != holding.end())
        holding.erase(it);
}

void state::fill_kbd_state(unsigned char* data) {
    auto& vec = updating ? (conf::get().is_replay ? repl_holding : holding) : st.prev;
    for (auto& val : vec)
        data[val] = 1;
}

void state::draw_info() { ImGui::Text("Frames: %i / %i", st.frames, st.total); }
