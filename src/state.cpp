#define WIN32_LEAN_AND_MEAN
#include "state.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "ofs.hpp"
#include "plugbase.hpp"
#include "files.hpp"
#include "timehooks.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vector>
#undef max
#undef min

constexpr int save_version = 1;

using std::string;

extern BOOL(WINAPI* QueryPerformanceFrequencyO)(LARGE_INTEGER* lpFrequency);
extern BOOL(WINAPI* QueryPerformanceCounterO)(LARGE_INTEGER* lpFrequency);

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

    void trim() {
        total = frames;
        // TODO: optimize
        while (!ev.empty() && (ev.end() - 1)->frame >= frames)
            ev.erase(ev.end() - 1);
    }
};

static State st;
static std::vector<int> holding;
static std::vector<int> repl_holding;
static LARGE_INTEGER last_counter;
static double const_dt;
static double to_wait;
static double freq;
static string last_msg;
static int64_t time_offset;
static bool success;
static bool updating;
static bool need_key_msg;

void state::init() {
    need_key_msg = plug::get().need_key_message;
    last_msg = "None";
    success = false;
    updating = false;
    st.fps = conf::get().fps;
    const_dt = 1.0 / (double)st.fps;
    to_wait = 0.0;
    time_offset = 0;
    spdlog::debug("Init FPS: {}", st.fps);
    QueryPerformanceFrequencyO(&last_counter);
    freq = (double)last_counter.QuadPart;
    QueryPerformanceCounterO(&last_counter);
}

void state::save_state(int slot) {
    string fp = string(files::get_cwd()) + "\\state_" + std::to_string(slot) + ".ostate";
    ofs::File file(fp, 1);
    if (!file.is_open()) {
        spdlog::warn("Failed to open state slot {} for writing", slot);
        return;
    }
    if (!save_game(file)) {
        spdlog::warn("Failed to save game state to slot {}", slot);
        file.close();
        ofs::remove_file(fp);
    }
}

void state::load_state(int slot) {
    ofs::File file(string(files::get_cwd()) + "\\state_" + std::to_string(slot) + ".ostate", 0);
    if (!file.is_open()) {
        spdlog::warn("Failed to open state slot {} for reading", slot);
        return;
    }
    if (!load_game(file))
        spdlog::warn("Failed to load game state from slot {}", slot);
}

void state::invalidate_process() { success = false; }

void state::early_update() {}

void state::before_update() {
    auto& cfg = conf::get();
    if (cfg.is_paused && !cfg.need_advance)
        return;
    updating = true;
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
    auto& cfg = conf::get();
    if (updating) {
        updating = false;
        auto prev_time = get_time(TimeOffset::None);
        st.frames++;
        timehooks::update(static_cast<int>(get_time(TimeOffset::None) - prev_time));
        st.total = std::max(st.total, st.frames);
    }
    if (cfg.fast_forward)
        return;
    if (to_wait < 0.0)
        to_wait = 0.0;
    to_wait += const_dt;
    if (to_wait > 1.0)
        to_wait = 1.0;
    while (to_wait > 0.0) {
        LARGE_INTEGER now_counter;
        QueryPerformanceCounterO(&now_counter);
        double dt = (double)(now_counter.QuadPart - last_counter.QuadPart) / freq;
        last_counter = now_counter;
        to_wait -= dt;
    }
}

int64_t state::get_utc_offset() {
    return static_cast<int64_t>(st.local_offset) - static_cast<int64_t>(st.system_offset);
}

uint64_t state::get_time(TimeOffset offset) {
    auto fps = st.fps;
    uint64_t ret = static_cast<uint64_t>(static_cast<int64_t>(st.frames) * 1000 / fps + time_offset);

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
        ASS(false);
        return 0;
    }
}

void state::set_temp_time_offset(int ms) {
    time_offset = static_cast<int64_t>(ms);
}

bool state::save_game(ofs::File& file) {
    ASS(file.is_open());
    success = true;
    return plug::get().save_state(file) && success;
}

bool state::load_game(ofs::File& file) {
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

void state::draw_info() {
    void* pGlobalApp = plug::get().get_prop(plug::PtrProp::PGlobalApp);
    int* scene_id =
        reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSceneID, pGlobalApp));
    ImGui::Text("Frames: %i / %i", st.frames, st.total);
    ImGui::Text("Scene: %i", scene_id ? *scene_id : -1);
    ImGui::Text("Message: %s", last_msg.c_str());
}
