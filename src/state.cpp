#define WIN32_LEAN_AND_MEAN
#include "state.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "event.hpp"
#include "files.hpp"
#include "ofs.hpp"
#include "plugbase.hpp"
#include "timehooks.hpp"
#include "video.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#undef max
#undef min

constexpr int save_version = 1;
constexpr int replay_version = 1;

using event::Event;
using ost::string_view;
using std::string;

extern HWND hwnd;
extern BOOL(WINAPI* QueryPerformanceFrequencyO)(LARGE_INTEGER* lpFrequency);
extern BOOL(WINAPI* QueryPerformanceCounterO)(LARGE_INTEGER* lpFrequency);

struct State {
    std::vector<Event> ev;
    std::vector<Event> temp_ev;
    std::vector<int> prev_kbd;
    int scene;
    int fps;
    int frames;
    int total;

    State() : scene(0), fps(0), frames(0), total(0) {}

    void clear_values() {
        scene = 0;
        frames = total = 0;
        fps = 0;
    }

    void clear_lists() {
        ev.clear();
        temp_ev.clear();
        prev_kbd.clear();
    }

    void trim() {
        total = frames;
        auto it = std::lower_bound(ev.begin(), ev.end(), frames,
                                   [](const Event& e, int f) { return e.frame < f; });
        if (it != ev.end())
            ev.erase(it, ev.end());
    }

    size_t calc_current_index() {
        auto it = std::lower_bound(ev.begin(), ev.end(), frames,
                                   [](const Event& e, int f) { return e.frame < f; });
        return std::distance(ev.begin(), it);
    }

    // TODO: get currently hold keys at the frame st.frames (so ENSURE->ASS in key sim)
};

static State st;
static std::vector<int> holding;
static std::vector<int> repl_holding;
static LARGE_INTEGER last_counter;
static double const_dt;
static double to_wait;
static double freq;
static string last_msg;
static string state_error_text;
static string base_path;
static size_t repl_index;
static int64_t time_offset;
static int need_scene_slot;
static uint64_t rerec_count;
static bool processing_save;
static bool updating;
static bool need_key_msg;
static void* temp_handle;

static uint64_t get_rerecords() {
    ofs::File file;
    uint64_t ret = 0;
    if (file.open(base_path + "\\rerecords.ofbin", 0)) {
        string line;
        if (file.readln(line))
            ret = std::stoull(line);
        file.close();
    }
    return ret;
}

static void set_rerecords(uint64_t count) {
    ofs::File file;
    if (file.open(base_path + "\\rerecords.ofbin", 1))
        file.writeln(std::to_string(count));
}

void state::init() {
    base_path = string(files::get_cwd()) + '\\' + conf::get().project_name;
    need_key_msg = plug::get().need_key_message;
    last_msg = "None";
    processing_save = false;
    updating = false;
    st.fps = conf::get().fps;
    const_dt = 1.0 / (double)st.fps;
    to_wait = 0.0;
    time_offset = 0;
    repl_index = 0;
    need_scene_slot = -10000;
    rerec_count = get_rerecords();
    spdlog::debug("Init FPS: {}", st.fps);
    QueryPerformanceFrequencyO(&last_counter);
    freq = (double)last_counter.QuadPart;
    QueryPerformanceCounterO(&last_counter);
}

void state::set_last_msg(ost::string_view msg) { last_msg = msg; }

bool state::is_save_handle(void* handle) {
    /* return processing_save;*/
    return processing_save && handle == temp_handle;
}

bool state::is_processing_save() { return processing_save; }

void state::save_state(int slot) {
    auto& cfg = conf::get();
    string fp = base_path + "\\state_" + std::to_string(slot) + ".ofstate";
    ofs::File file;
    if (!file.open(fp, 1, false)) {
        spdlog::warn("Failed to open state slot {} for writing", slot);
        return;
    }
    temp_handle = file.get_handle();
    auto bool_ret = file.write("ofstate!", 8);
    ENSURE(bool_ret);
    write_bin(file, save_version);
    write_bin(file, st.scene);
    write_bin(file, st.frames);
    write_bin(file, st.total);
    write_bin(file, st.fps);
    write_bin(file, st.prev_kbd);
    write_bin(file, st.temp_ev);
    write_bin(file, st.ev);
    processing_save = true;
    auto ret = plug::get().save_state(file);
    if (!ret.has_value()) {
        processing_save = false;
        spdlog::warn("Failed to save state data: {}", ret.error());
        last_msg = "Failed to save state data: " + ret.error();
        file.close();
        ofs::remove_file(fp);
        return;
    }
    if (cfg.save_game_state && !processing_save) {
        spdlog::warn("Failed to save game state: {}", state_error_text);
        last_msg = "Failed to save game state: " + state_error_text;
        return;
    }
    processing_save = false;
    last_msg = string("State ") + std::to_string(slot) + " saved!";
}

void state::load_state(int slot) {
    need_scene_slot = -10000;
    auto& cfg = conf::get();
    if (cfg.is_replay && cfg.reset_on_replay && st.frames != 0) {
        // Need to restart game before replay
        need_scene_slot = slot;
        reset_game();
        last_msg = "Restarting game";
        return;
    }
    ofs::File file(base_path + "\\state_" + std::to_string(slot) + ".ofstate", 0);
    if (!file.is_open()) {
        spdlog::warn("Failed to open state slot {} for reading", slot);
        return;
    }
    temp_handle = file.get_handle();
    static char test_buf[8];
    if (!file.read(test_buf, 8) || memcmp(test_buf, "ofstate!", 8) != 0) {
        spdlog::error("Invalid state file");
        return;
    }
    int int_data;
    load_bin(file, int_data);
    if (int_data != save_version) {
        spdlog::error("State version mismatch (expected {}, got {})", save_version, int_data);
        return;
    }
    State temp_state;
    // State& temp_state = st;
    load_bin(file, temp_state.scene);
    if (!cfg.is_replay && temp_state.scene != st.scene) {
        need_scene_slot = slot;
        spdlog::debug("Preparing scene change: {} -> {}", st.scene, temp_state.scene);
        last_msg = "Changing scene: " + std::to_string(st.scene) + " -> " +
                   std::to_string(temp_state.scene);
        void* pState = plug::get().get_prop(plug::PtrProp::PState);
        short* ptr =
            reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameTask, pState));
        *ptr = 3;
        ptr = reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameData, pState));
        *ptr = static_cast<short>(temp_state.scene) | 0x8000;
        return;
    }
    load_bin(file, temp_state.frames);
    load_bin(file, temp_state.total);
    load_bin(file, temp_state.fps);
    load_bin(file, temp_state.prev_kbd);
    load_bin(file, temp_state.temp_ev);
    load_bin(file, temp_state.ev);
    int prev_frames = st.frames;
    if (!cfg.is_replay) {
        st.frames = temp_state.frames; // Need to set before load_state
        processing_save = true;
    }
    auto ret = plug::get().load_state(file);
    if (!ret.has_value()) {
        processing_save = false;
        st.frames = prev_frames;
        spdlog::warn("Failed to load state data: {}", ret.error());
        last_msg = "Failed to load state data: " + ret.error();
        return;
    }
    if (!cfg.is_replay && !processing_save) {
        st.frames = prev_frames;
        spdlog::warn("Failed to restore game state: {}", state_error_text);
        last_msg = "Failed to restore game state: " + state_error_text;
        return;
    }
    if (cfg.is_replay) {
        st.ev = std::move(temp_state.ev);
        st.total = temp_state.total;
        repl_index = st.calc_current_index();
    } else {
        st = std::move(temp_state);
        // TODO: maybe trim on the first frame, not directly when loading?
        st.trim();
        repl_index = 0;
    }
    set_rerecords(++rerec_count);
    processing_save = false;
    last_msg = string("State ") + std::to_string(slot) + " loaded!";
}

bool state::invalidate_process(string_view text) {
    if (!processing_save)
        return false;
    state_error_text = text;
    processing_save = false;
    return true;
}

static void exec_event(Event ev) {
    switch (ev.idx) {
    case 1:
        if (ev.key.down == true) {
            auto it = std::find(repl_holding.begin(), repl_holding.end(), ev.key.k);
            if (it == repl_holding.end()) {
                repl_holding.push_back(ev.key.k);
                if (need_key_msg)
                    winhooks::sim_key_event(ev.key.k, true);
            } else
                spdlog::error("Failed to simulate key: key {} is already down", ev.key.k);
        } else {
            auto it = std::find(repl_holding.begin(), repl_holding.end(), ev.key.k);
            if (it != repl_holding.end()) {
                repl_holding.erase(it);
                if (need_key_msg)
                    winhooks::sim_key_event(ev.key.k, false);
            } else
                spdlog::error("Failed to simulate key: key {} is already up", ev.key.k);
        }
        break;
    default:
        // TODO: custom events for plugins
        ASS(false);
        break;
    }
}

void state::early_update() { updating = false; }

bool state::before_update() {
    auto& cfg = conf::get();
    ASS(need_scene_slot == -10000 || need_scene_slot >= 0);
    if (need_scene_slot != -10000) {
        load_state(need_scene_slot);
        // cfg.is_paused = true;
        // cfg.need_advance = false;
    }
    void* pGlobalApp = plug::get().get_prop(plug::PtrProp::PGlobalApp);
    ASS(pGlobalApp != nullptr);
    int* pScene = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSceneID, pGlobalApp));
    ASS(pScene != nullptr);
    st.scene = *pScene;
    if (!cfg.is_paused && IsIconic(::hwnd))
        cfg.is_paused = true;
    if ((cfg.is_paused && !cfg.need_advance) || need_scene_slot != -10000)
        return false;
    updating = true;
    if (cfg.is_replay) {
        for (; repl_index < st.ev.size(); repl_index++) {
            Event& ev = st.ev[repl_index];
            if (ev.frame > st.frames)
                break;
            if (ev.frame < st.frames)
                continue;
            exec_event(ev);
        }
    } else {
        for (auto it = holding.begin(); it != holding.end(); it++) {
            auto pit = std::find(st.prev_kbd.begin(), st.prev_kbd.end(), *it);
            if (pit == st.prev_kbd.end()) {
                // Down event
                if (need_key_msg)
                    winhooks::sim_key_event(*it, true);
                Event event;
                event.frame = st.frames;
                event.idx = 1;
                event.key.k = static_cast<short>(*it);
                event.key.down = true;
                st.ev.push_back(event);
            }
        }
        for (auto pit = st.prev_kbd.begin(); pit != st.prev_kbd.end(); pit++) {
            auto it = std::find(holding.begin(), holding.end(), *pit);
            if (it == holding.end()) {
                // Up event
                if (need_key_msg)
                    winhooks::sim_key_event(*pit, false);
                Event event;
                event.frame = st.frames;
                event.idx = 1;
                event.key.k = static_cast<short>(*pit);
                event.key.down = false;
                st.ev.push_back(event);
            }
        }
    }
    st.prev_kbd = cfg.is_replay ? repl_holding : holding;
    return true;
}

void state::after_update() {
    auto& cfg = conf::get();
    if (updating) {
        updating = false;
        auto prev_time = get_time(TimeOffset::None);
        st.frames++;
        timehooks::update(static_cast<int>(get_time(TimeOffset::None) - prev_time));
        st.total = std::max(st.total, st.frames);
        if (cfg.is_replay && st.frames == st.total && st.frames > 0) {
            cfg.is_paused = true;
            cfg.is_replay = false;
            on_mode_switch();
            last_msg = "Switched to recording";
        }
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
    auto& cfg = conf::get();
    return static_cast<int64_t>(cfg.local_offset) - static_cast<int64_t>(cfg.system_offset);
}

uint64_t state::get_time(TimeOffset offset) {
    auto& cfg = conf::get();
    auto fps = static_cast<int64_t>(st.fps);
    uint64_t ret =
        static_cast<uint64_t>(static_cast<int64_t>(st.frames) * 1000 / fps + time_offset);
    switch (offset) {
    case TimeOffset::None:
        return ret;
    case TimeOffset::System:
        return ret + cfg.system_offset;
    case TimeOffset::Local:
        return ret + cfg.local_offset;
    case TimeOffset::Startup:
        return ret + cfg.startup_offset;
    case TimeOffset::Reminder:
        return static_cast<uint64_t>(st.frames) * 1000 % fps;
    default:
        ASS(false);
        return 0;
    }
}

void state::set_temp_time_offset(int ms) { time_offset = static_cast<int64_t>(ms); }

bool state::get_key_state(int vk) {
    return std::find(st.prev_kbd.begin(), st.prev_kbd.end(), vk) != st.prev_kbd.end();
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
    for (auto& val : st.prev_kbd)
        data[val] = 1;
}

void state::on_mode_switch() {
    auto& cfg = conf::get();
    repl_holding.clear(); // TODO: is this right????
    if (cfg.is_replay) {
        repl_index = st.calc_current_index();
    } else {
        // TODO: maybe trim on the first frame, not directly when loading?
        st.trim();
        repl_index = 0;
    }
}

void state::draw_info() {
    auto& cfg = conf::get();
    ImGui::Text("%s%s", cfg.is_replay ? "[REPLAY]" : "[RECORD]",
                video::is_recording() ? " [VIDEO]" : "");
    ImGui::Text("Frames: %i / %i", st.frames, st.total);
    ImGui::Text("Scene: %i", st.scene);
    ImGui::Text("Re-records: %llu", rerec_count);
#ifdef _DEBUG
    ImGui::Text("Replay index: %i", repl_index);
    ImGui::Text("Event count: %llu", static_cast<uint64_t>(st.ev.size()));
#endif
    ImGui::Text("Message: %s", last_msg.c_str());
    if (!cfg.fast_forward) {
        ImGui::Text("Keys: TODO");
    }
}

void state::reset_game() {
    st.prev_kbd.clear();
    st.ev.clear();
    st.frames = 0;
    st.total = 0;
    st.prev_kbd.clear();
    repl_holding.clear();
    repl_index = 0;
    void* pState = plug::get().get_prop(plug::PtrProp::PState);
    short* ptr =
        reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameTask, pState));
    *ptr = 4;
    ptr = reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameData, pState));
    *ptr = 0;
}
