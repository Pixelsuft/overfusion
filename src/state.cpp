#define WIN32_LEAN_AND_MEAN
#include "state.hpp"
#include "ass.hpp"
#include "audio.hpp"
#include "config.hpp"
#include "event.hpp"
#include "files.hpp"
#include "input.hpp"
#include "ofs.hpp"
#include "plugbase.hpp"
#include "timehooks.hpp"
#include "video.hpp"
#include <Windows.h>
#include <algorithm>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#undef max
#undef min

constexpr int save_version = 1;
constexpr int replay_version = 1;
constexpr int empty_save_slot = -10000;

using event::Event;
using ost::string_view;
using std::string;

extern HWND hwnd;
extern BOOL(WINAPI* QueryPerformanceFrequencyO)(LARGE_INTEGER* lpFrequency);
extern BOOL(WINAPI* QueryPerformanceCounterO)(LARGE_INTEGER* lpFrequency);

struct State {
    // Event list
    std::vector<Event> ev;
    // TODO: maybe keep keydowns here instead of holding?
    // and clear them when saving state?
    // or just duplicate them here? (this one might be the best)
    // Temp event list
    std::vector<Event> temp_ev;
    // Holding inputs last frame
    std::vector<int> prev_input;
    // Re-record count
    uint64_t rerec_count;
    // Normalized mouse pos
    std::pair<float, float> mouse_pos;
    // Current scene ID
    int scene;
    // Current frame
    int frames;
    // Total frames
    int total;

    State() : scene(0), frames(0), total(0), rerec_count(0), mouse_pos({-1.f, -1.f}) {}

    void clear_values() {
        scene = 0;
        frames = total = 0;
        mouse_pos.first = mouse_pos.second = -1.f;
    }

    void clear_lists() {
        ev.clear();
        temp_ev.clear();
        prev_input.clear();
    }

    // Trim total frames to current frames
    void trim() {
        total = frames;
        auto it = std::lower_bound(ev.begin(), ev.end(), frames,
                                   [](const Event& e, int f) { return e.frame < f; });
        if (it != ev.end())
            ev.erase(it, ev.end());
    }

    // Calculate replay_index based on a current frame
    size_t calc_current_index() {
        auto it = std::lower_bound(ev.begin(), ev.end(), frames,
                                   [](const Event& e, int f) { return e.frame < f; });
        return std::distance(ev.begin(), it);
    }

    // Calculate holding keys based on a current frame
    std::vector<int> calc_keys() {
        std::vector<int> ret;
        for (auto& e : ev) {
            if (e.frame >= frames)
                break;
            if (e.idx != event::Type::KeyDown)
                continue;
            auto it = std::find(ret.begin(), ret.end(), e.key.k);
            if (e.key.down) {
                if (it == ret.end())
                    ret.push_back(e.key.k);
                else
                    spdlog::error("Key {} is double-pressed at frame {}", e.key.k, e.frame);
            } else {
                if (it != ret.end())
                    ret.erase(it);
                else
                    spdlog::error("Key {} is double-released at frame {}", e.key.k, e.frame);
            }
        }
        return ret;
    }
};

namespace state {
// Current state
static State st;
// Real holding keys
static std::vector<int> real_holding;
// Replay/real at the current frame holding keys
static std::vector<int> cur_holding;
// Ugly needed buffer for message boxes
static std::vector<int> msgbox_buf;
// Manual waiting stuff
static LARGE_INTEGER last_counter;
static double const_dt;
static double to_wait;
static double freq;
// Last info
static string last_msg;
// For displaying save/load error
static string state_error_text;
// Project path
static string base_path;
// Current event id (index)
static size_t repl_index;
// Time offset
static int64_t time_offset;
// Remember needed scene before switching if needed
static int need_scene_slot;
// Are we saving/loading?
static bool processing_save;
// Are we processing game frame?
static bool updating;
// Save file handle
static void* temp_handle;
} // namespace state

inline int str_to_int(const std::string& str) {
    // No exceptions please
    char* endptr;
    long num = std::strtol(str.c_str(), &endptr, 10);
    return endptr != str.c_str() ? static_cast<int>(num) : 0;
}

inline float str_to_float(const std::string& str) noexcept {
    // No exceptions please
    char* endptr = nullptr;
    float num = std::strtof(str.c_str(), &endptr);
    return endptr != str.c_str() ? num : 0.0f;
}

static uint64_t get_rerecords() {
    ofs::File file;
    uint64_t ret = 0;
    if (file.open(state::base_path + "\\rerecords.ofbin", 0)) {
        string line;
        if (file.readln(line))
            ret = static_cast<uint64_t>(str_to_int(line));
        file.close();
    }
    return ret;
}

static void set_rerecords(uint64_t count) {
    ofs::File file;
    if (file.open(state::base_path + "\\rerecords.ofbin", 1))
        file.writeln(std::to_string(count));
}

void state::init() {
    base_path = string(files::get_cwd()) + '\\' + conf::get().project_name;
    last_msg = "None";
    processing_save = false;
    updating = false;
    const_dt = 1.0 / static_cast<double>(conf::get().fps);
    to_wait = 0.0;
    time_offset = 0;
    repl_index = 0;
    need_scene_slot = empty_save_slot;
    st.temp_ev.reserve(1024);
    st.ev.reserve(4096);
    QueryPerformanceFrequencyO(&last_counter);
    freq = static_cast<double>(last_counter.QuadPart);
    QueryPerformanceCounterO(&last_counter);
}

void state::set_last_msg(string_view msg) { last_msg = string(msg); }

bool state::is_save_handle(void* handle) {
    // For fast checking file hooks to skip our handle
    return processing_save && handle == temp_handle;
}

bool state::is_processing_save() { return processing_save; }

void state::export_replay(string_view fn) {
    last_msg.clear();
    ofs::File file;
    if (!file.open(base_path + '\\' + string(fn), 1, false)) {
        spdlog::error("Failed to open replay file \"{}\" for writing", fn);
        last_msg = "Failed to save replay file";
        return;
    }
    auto& cfg = conf::get();
    auto fret = file.writeln("-8,pixelsuft_overfusion," + std::to_string(replay_version));
    ENSURE(fret);
    fret = file.writeln("-7,total," + std::to_string(st.total));
    ENSURE(fret);
    fret = file.writeln("-6,rerecords," + std::to_string(st.rerec_count));
    ENSURE(fret);
    fret = file.writeln("-5,fps," + std::to_string(cfg.fps));
    ENSURE(fret);
    fret = file.writeln("-4,system_offset," + std::to_string(cfg.system_offset));
    ENSURE(fret);
    fret = file.writeln("-3,local_offset," + std::to_string(cfg.local_offset));
    ENSURE(fret);
    fret = file.writeln("-2,startup_offset," + std::to_string(cfg.startup_offset));
    ENSURE(fret);
    fret = file.writeln("-1,events_begin,0");
    ENSURE(fret);
    for (const auto& e : st.ev) {
        int int_idx = static_cast<int>(e.idx);
        switch (e.idx) {
        case event::Type::KeyDown:
        case event::Type::MouseDown:
            fret = file.writeln(std::to_string(e.frame) + ',' + std::to_string(int_idx) + ',' +
                                std::to_string(e.key.k) + ',' + std::to_string(e.key.down ? 1 : 0));
            ENSURE(fret);
            break;
        case event::Type::MouseMove:
            fret = file.writeln(std::to_string(e.frame) + ',' + std::to_string(int_idx) + ',' +
                                std::to_string(e.mouse.x * 1000.f) + ',' +
                                std::to_string(e.mouse.y * 1000.f));
            ENSURE(fret);
            break;
        case event::Type::MsgBox:
            fret = file.writeln(std::to_string(e.frame) + ',' + std::to_string(int_idx) + ',' +
                                std::to_string(e.msgbox.choice));
            ENSURE(fret);
            break;
        case event::Type::None:
            ASS(false);
            break;
        default:
            ASS(false);
            break;
        }
    }
    last_msg = "Replay exported";
    spdlog::info("Replay exported");
}

void state::import_replay(string_view fn) {
    auto& cfg = conf::get();
    ofs::File file;
    last_msg.clear();
    if (!file.open(base_path + '\\' + string(fn), 0, false)) {
        spdlog::error("Failed to open replay file \"{}\" for reading", fn);
        last_msg = "Failed to open replay file";
        return;
    }
    State temp_state;
    string line;
    bool is_of = false;
    while (file.readln(line)) {
        if (line.size() < 3)
            continue;
        auto start = line.find(',');
        if (start == string::npos) {
            spdlog::error("Failed to parse replay header (frame)");
            is_of = false;
            break;
        }
        start++;
        auto end = line.find(',', start);
        if (end == string::npos) {
            spdlog::error("Failed to parse replay header (name)");
            is_of = false;
            break;
        }
        auto sub = line.substr(start, end - start);
        end++;
        auto sub2 = line.substr(end);
        if (sub == "pixelsuft_overfusion") {
            int st_ver = str_to_int(sub2);
            if (st_ver != replay_version) {
                spdlog::error("Invalid replay version (expected {}, got {})", replay_version,
                              st_ver);
                is_of = false;
                break;
            }
            is_of = true;
        } else if (sub == "total") {
            temp_state.total = str_to_int(sub2);
        } else if (sub == "rerecords") {
            temp_state.rerec_count = std::max(str_to_int(sub2), 0);
        } else if (sub == "fps") {
        } else if (sub == "system_offset") {
        } else if (sub == "local_offset") {
        } else if (sub == "startup_offset") {
        } else if (sub == "events_begin") {
            break;
        } else {
            is_of = false;
            spdlog::error("Invalid replay header");
            break;
        }
    }
    if (temp_state.total < 0)
        is_of = false;
    if (!is_of) {
        last_msg = "Invalid replay file";
        spdlog::error("Invalid replay file");
        return;
    }
    bool need_sort = false;
    while (file.readln(line)) {
        if (line.size() < 3)
            continue;
        Event event;
        auto end = line.find(',');
        if (end == string::npos || end == 0) {
            spdlog::error("Invalid event line (frame)");
            continue;
        }
        event.frame = str_to_int(line.substr(0, end));
        end++;
        auto start = end;
        end = line.find(',', start);
        if (end == string::npos || end == start) {
            spdlog::error("Invalid event line (event index)");
            continue;
        }
        event.idx = static_cast<event::Type>(str_to_int(line.substr(start, end)));
        end++;
        switch (event.idx) {
        case event::Type::KeyDown:
        case event::Type::MouseDown:
            start = end;
            end = line.find(',', start);
            if (end == string::npos) {
                spdlog::error("Invalid keyboard/mouse event line (key)");
                continue;
            }
            event.key.k = str_to_int(line.substr(start, end));
            if (event.key.k <= 0) {
                spdlog::error("Invalid keyboard/mouse event (key)");
                continue;
            }
            event.key.down = str_to_int(line.substr(++end)) != 0;
            break;
        case event::Type::MouseMove:
            start = end;
            end = line.find(',', start);
            if (end == string::npos) {
                spdlog::error("Invalid mouse move event line (X)");
                continue;
            }
            event.mouse.x = str_to_float(line.substr(start, end)) / 1000.f;
            event.mouse.y = str_to_float(line.substr(++end)) / 1000.f;
            break;
        case event::Type::MsgBox:
            event.msgbox.choice = str_to_int(line.substr(end));
            if (event.msgbox.choice <= 0) {
                spdlog::error("Invalid message box event line (choice)");
                continue;
            }
            break;
        case event::Type::None:
        default:
            spdlog::warn("Invalid event index, skipping");
            break;
        }
        if (!temp_state.ev.empty() && temp_state.ev.back().frame > event.frame) {
            spdlog::warn("Invalid frame index order, fixing");
            need_sort = true;
        }
        temp_state.ev.push_back(event);
    }
    if (need_sort)
        std::stable_sort(temp_state.ev.begin(), temp_state.ev.end(),
                         [](const Event& a, const Event& b) { return a.frame < b.frame; });
    if (!temp_state.ev.empty() && temp_state.ev.back().frame >= temp_state.total) {
        spdlog::warn("Invalid total frames counter, fixing");
        temp_state.total = temp_state.ev.back().frame + 1;
    }
    st.ev = std::move(temp_state.ev);
    st.total = temp_state.total;
    st.rerec_count = temp_state.rerec_count;
    repl_index = st.calc_current_index();
    auto new_holding = st.calc_keys();
    if (new_holding != st.prev_input) {
        spdlog::warn("Keyboard state mismatch during replay load, fixing");
        st.prev_input = std::move(new_holding);
    }
    cfg.is_replay = true;
    last_msg = "Replay imported";
    spdlog::info("Replay imported");
}

void state::save_state(int slot) {
    last_msg.clear();
    auto& cfg = conf::get();
    string fp = base_path + "\\state_" + std::to_string(slot) + ".ofstate";
    ofs::File file;
    if (!file.open(fp, 1, false)) {
        spdlog::error("Failed to open state slot {} for writing", slot);
        last_msg = "Failed to save state from slot " + std::to_string(slot) + " to file";
        return;
    }
    temp_handle = file.get_handle();
    auto bool_ret = file.write("ofstate!", 8);
    ENSURE(bool_ret);
    write_bin(file, save_version);
    write_bin(file, st.scene);
    write_bin(file, st.frames);
    write_bin(file, st.total);
    write_bin(file, st.rerec_count);
    write_bin(file, st.mouse_pos);
    write_bin(file, st.prev_input);
    write_bin(file, st.temp_ev);
    write_bin(file, st.ev);
    processing_save = true;
    auto ret = plug::get().save_state(file);
    if (!ret.has_value()) {
        // Plugin failed to save it's data
        processing_save = false;
        spdlog::error("Failed to save state data: {}", ret.error());
        last_msg = "Failed to save state data: " + ret.error();
        file.close();
        ofs::remove_file(fp);
        return;
    }
    if (cfg.save_game_state && !processing_save) {
        // Game failed to save it's data
        spdlog::error("Failed to save game state: {}", state_error_text);
        last_msg = "Failed to save game state: " + state_error_text;
        return;
    }
    // Ok, let's do this here
    bool_ret = files::save_fs(file);
    ENSURE(bool_ret);
    processing_save = false;
    if (last_msg.empty())
        last_msg = string("State ") + std::to_string(slot) + " saved!";
    else
        last_msg = string("State ") + std::to_string(slot) + " saved! (" + last_msg + ")";
    spdlog::info("State saved");
}

void state::load_state(int slot, bool no_recursion) {
    last_msg.clear();
    need_scene_slot = empty_save_slot;
    auto& cfg = conf::get();
    if (cfg.is_replay && cfg.reset_on_replay && st.frames != 0 && !no_recursion) {
        // Need to restart game before replay
        need_scene_slot = slot;
        if (!plug::get().set_trans_enabled(false))
            spdlog::debug("Failed to set transitions disabled");
        reset_game();
        last_msg = "Restarting game";
        return;
    }
    ofs::File file(base_path + "\\state_" + std::to_string(slot) + ".ofstate", 0);
    if (!file.is_open()) {
        spdlog::warn("Failed to open state slot {} for reading", slot);
        last_msg = "Failed to open state slot " + std::to_string(slot);
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
    // Create temp_state and put it into st only without errors
    State temp_state;
    // State& temp_state = st;
    load_bin(file, temp_state.scene);
    if (!cfg.is_replay && temp_state.scene != st.scene && !no_recursion) {
        // Need to switch scene before loading state
        need_scene_slot = slot;
        if (!plug::get().set_trans_enabled(false))
            spdlog::debug("Failed to set transitions disabled");
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
    load_bin(file, temp_state.rerec_count);
    load_bin(file, temp_state.mouse_pos);
    load_bin(file, temp_state.prev_input);
    load_bin(file, temp_state.temp_ev);
    load_bin(file, temp_state.ev);
    int prev_frames = st.frames;
    if (!cfg.is_replay) {
        st.frames = temp_state.frames; // Need to set before load_state
        processing_save = true;
    }
    auto ret = plug::get().load_state(file);
    if (!ret.has_value()) {
        // State failed to load it's data
        processing_save = false;
        st.frames = prev_frames;
        spdlog::error("Failed to load state data: {}", ret.error());
        last_msg = "Failed to load state data: " + ret.error();
        return;
    }
    if (!cfg.is_replay && !processing_save) {
        // Game failed to load it's data
        st.frames = prev_frames;
        spdlog::error("Failed to restore game state: {}", state_error_text);
        last_msg = "Failed to restore game state: " + state_error_text;
        return;
    }
    if (cfg.is_replay) {
        st.ev = std::move(temp_state.ev);
        st.total = temp_state.total;
        st.rerec_count = temp_state.rerec_count;
        repl_index = st.calc_current_index();
        auto new_holding = st.calc_keys();
        if (new_holding != st.prev_input) {
            spdlog::warn("Keyboard state mismatch during replay load, fixing");
            st.prev_input = std::move(new_holding);
            last_msg = "Keyboard mismatch fixed";
        }
    } else {
        st = std::move(temp_state);
        // TODO: maybe trim on the first frame, not directly when loading?
        st.trim();
        repl_index = st.calc_current_index();
        // FIXME, files get resored even if load state fails
        auto bool_ret = files::load_fs(file);
        ENSURE(bool_ret);
    }
    st.rerec_count = std::max(st.rerec_count, get_rerecords()) + 1;
    set_rerecords(st.rerec_count);
    processing_save = false;
    if (last_msg.empty())
        last_msg = string("State ") + std::to_string(slot) + " loaded!";
    else
        last_msg = string("State ") + std::to_string(slot) + " loaded! (" + last_msg + ")";
    spdlog::info("State loaded");
}

bool state::invalidate_process(string_view text) {
    if (!processing_save)
        return false;
    state_error_text = string(text);
    processing_save = false;
    return true;
}

int state::process_message_box(ost::string_view text, ost::string_view caption,
                               unsigned int uType) {
    if (uType == 0x30 && invalidate_process(text))
        return IDOK;
    auto& cfg = conf::get();
    spdlog::info("MessageBox: {} - {}", caption, text);
    if (!cfg.is_replay)
        return 0;
    ENSURE(!msgbox_buf.empty());
    if (msgbox_buf.empty())
        return 0;
    auto ret = msgbox_buf.front();
    msgbox_buf.erase(msgbox_buf.begin());
    return ret;
}

void state::remember_message_box(int choice) {
    if (!conf::get().is_replay) {
        Event event;
        event.frame = st.frames;
        event.idx = event::Type::MsgBox;
        event.msgbox.choice = choice;
        st.ev.push_back(event);
    }
}

static bool exec_event(Event ev) {
    switch (ev.idx) {
    case event::Type::KeyDown: {
        auto it = std::find(state::cur_holding.begin(), state::cur_holding.end(), ev.key.k);
        // Key down/up
        if (ev.key.down == true) {
            auto it = std::find(state::cur_holding.begin(), state::cur_holding.end(), ev.key.k);
            if (it == state::cur_holding.end()) {
                state::cur_holding.push_back(ev.key.k);
                input::sim_key_event(ev.key.k, true);
                return true;
            } else {
                spdlog::warn("Failed to simulate key: key {} is already down", ev.key.k);
                return false;
            }
        } else {
            if (it != state::cur_holding.end()) {
                state::cur_holding.erase(it);
                input::sim_key_event(ev.key.k, false);
                return true;
            } else {
                spdlog::warn("Failed to simulate key: key {} is already up", ev.key.k);
                return false;
            }
        }
    }
    case event::Type::MouseDown: {
        // Mouse down/up
        auto it = std::find(state::cur_holding.begin(), state::cur_holding.end(), ev.key.k);
        if (ev.key.down == true) {
            if (it == state::cur_holding.end()) {
                state::cur_holding.push_back(ev.key.k);
                input::sim_mouse_event(ev.key.k, true);
                return true;
            } else {
                spdlog::warn("Failed to simulate mouse: button {} is already down", ev.key.k);
                return false;
            }
        } else {
            if (it != state::cur_holding.end()) {
                state::cur_holding.erase(it);
                input::sim_mouse_event(ev.key.k, false);
                return true;
            } else {
                spdlog::warn("Failed to simulate mouse: button {} is already up", ev.key.k);
                return false;
            }
        }
    }
    case event::Type::MouseMove: {
        // Mouse move
        auto real_p = plug::get().mouse_to_window(ev.mouse.x, ev.mouse.y);
        state::st.mouse_pos.first = ev.mouse.x;
        state::st.mouse_pos.second = ev.mouse.y;
        input::sim_mouse_move(real_p.first, real_p.second);
        return true;
    }
    case event::Type::MsgBox: {
        // Message box user choice
        state::msgbox_buf.push_back(ev.msgbox.choice);
        return true;
    }
    case event::Type::None:
        ASS(false);
        return false;
    default:
        // TODO: custom events for plugins
        ASS(false);
        return false;
    }
}

void state::early_update() { updating = false; }

bool state::before_update(bool is_transitioning) {
    auto& cfg = conf::get();
    ASS(need_scene_slot == empty_save_slot || need_scene_slot >= 0);
    if (need_scene_slot != empty_save_slot) {
        if (is_transitioning) {
            cfg.need_advance = true;
        } else {
            load_state(need_scene_slot, true);
            cfg.is_paused = true;
            cfg.need_advance = false;
            if (!plug::get().set_trans_enabled(true))
                spdlog::debug("Failed to set transitions back enabled");
        }
    }
    void* pGlobalApp = plug::get().get_prop(plug::PtrProp::PGlobalApp);
    ASS(pGlobalApp != nullptr);
    int* pScene = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSceneID, pGlobalApp));
    ASS(pScene != nullptr);
    st.scene = *pScene;
    // Do not update when the game is minimized
    if (!cfg.is_paused && IsIconic(::hwnd))
        cfg.is_paused = true;
    if ((cfg.is_paused && !cfg.need_advance) || need_scene_slot != empty_save_slot)
        return false;
    updating = true;
    cur_holding = st.prev_input;
    msgbox_buf.clear();
    if (cfg.is_replay) {
        for (; repl_index < st.ev.size(); repl_index++) {
            Event& ev = st.ev[repl_index];
            if (ev.frame > st.frames)
                break;
            if (ev.frame < st.frames) {
                ASS(false);
                continue;
            }
            exec_event(ev);
        }
    } else {
        for (auto it = real_holding.begin(); it != real_holding.end(); it++) {
            auto pit = std::find(st.prev_input.begin(), st.prev_input.end(), *it);
            if (pit == st.prev_input.end()) {
                // Down event
                Event event;
                event.frame = st.frames;
                event.idx = event::Type::KeyDown;
                event.key.k = static_cast<short>(*it);
                event.key.down = true;
                st.temp_ev.push_back(event);
            }
        }
        for (auto pit = st.prev_input.begin(); pit != st.prev_input.end(); pit++) {
            auto it = std::find(real_holding.begin(), real_holding.end(), *pit);
            if (it == real_holding.end()) {
                // Up event
                Event event;
                event.frame = st.frames;
                event.idx = event::Type::KeyDown;
                event.key.k = static_cast<short>(*pit);
                event.key.down = false;
                st.temp_ev.push_back(event);
            }
        }
        st.ev.insert(st.ev.end(), st.temp_ev.begin(), st.temp_ev.end());
        for (auto& temp : st.temp_ev)
            exec_event(temp);
        st.temp_ev.clear();
    }
    st.prev_input = cur_holding;
    return true;
}

void state::after_update() {
    auto& cfg = conf::get();
    if (updating) {
        updating = false;
        auto prev_time = get_time(TimeOffset::None);
        st.frames++;
        // Update timers with "real" delta time
        timehooks::update(static_cast<int>(get_time(TimeOffset::None) - prev_time));
        st.total = std::max(st.total, st.frames);
        if (cfg.is_replay && st.frames == st.total && st.frames > 0) {
            cfg.is_paused = true;
            cfg.is_replay = false;
            ENSURE(repl_index == st.ev.size());
            on_mode_switch();
            last_msg = "Switched to recording";
        }
    }
    if (cfg.fast_forward)
        return;
    if (to_wait < 0.0)
        to_wait = 0.0;
    to_wait += const_dt / static_cast<double>(cfg.speed);
    if (to_wait > 1.0)
        to_wait = 1.0;
    while (to_wait > 0.0) {
        LARGE_INTEGER now_counter;
        QueryPerformanceCounterO(&now_counter);
        double dt = static_cast<double>(now_counter.QuadPart - last_counter.QuadPart) / freq;
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
    auto fps = static_cast<int64_t>(cfg.fps);
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
    return std::find(st.prev_input.begin(), st.prev_input.end(), vk) != st.prev_input.end();
}

void state::set_key_down(int vk, bool down) {
    ASS(vk > 0 && vk < 256);
    auto it = std::find(real_holding.begin(), real_holding.end(), vk);
    if (down) {
        if (it == real_holding.end())
            real_holding.push_back(vk);
        else
            spdlog::warn("Cannot double press key {}", vk);
    } else if (!down) {
        if (it != real_holding.end())
            real_holding.erase(it);
        // else
        //     spdlog::warn("Cannot double release key {}", vk);
    }
}

bool state::get_tas_mouse_down(int vk) {
    auto prev_it = std::find_if(st.temp_ev.rbegin(), st.temp_ev.rend(), [vk](const Event& te) {
        return te.idx == event::Type::MouseDown && te.key.k == vk;
    });
    if (prev_it == st.temp_ev.rend())
        return std::find(st.prev_input.begin(), st.prev_input.end(), vk) != st.prev_input.end();
    else
        return prev_it->key.down;
}

std::pair<float, float> state::get_tas_mouse_pos() {
    auto prev_it = std::find_if(st.temp_ev.rbegin(), st.temp_ev.rend(),
                                [](const Event& te) { return te.idx == event::Type::MouseMove; });
    if (prev_it == st.temp_ev.rend())
        return st.mouse_pos;
    else
        return {prev_it->mouse.x, prev_it->mouse.y};
}

void state::add_mouse_toggle(int vk) {
    Event event;
    event.frame = st.frames;
    event.idx = event::Type::MouseDown;
    event.key.k = vk;
    event.key.down = !get_tas_mouse_down(vk);
    st.temp_ev.push_back(event);
    last_msg = std::string("Queued mouse ") + (event.key.down ? "down" : "up");
}

void state::add_mouse_move() {
    auto real_p = input::get_real_mouse_pos();
    auto pos = plug::get().mouse_from_window(real_p.first, real_p.second);
    auto prev_pos = get_tas_mouse_pos();
    if (pos == prev_pos) {
        last_msg = "Mouse move skipped because position is the same";
        return;
    }
    Event event;
    event.frame = st.frames;
    event.idx = event::Type::MouseMove;
    event.mouse.x = pos.first;
    event.mouse.y = pos.second;
    st.temp_ev.push_back(event);
    last_msg = "Queued mouse move to (" + std::to_string(pos.first) + ", " +
               std::to_string(pos.second) + ") with (" + std::to_string(real_p.first) + ", " +
               std::to_string(real_p.second) + ")";
}

std::pair<int, int> state::get_mouse_pos() {
    /*ASS(std::find_if(st.temp_ev.begin(), st.temp_ev.end(), [](const Event& te) {
            return te.idx == event::Type::MouseMove;
            }) == st.temp_ev.end());*/
    return plug::get().mouse_to_window(st.mouse_pos.first, st.mouse_pos.second);
}

bool state::set_win_mouse_pos(int x, int y) {
    ASS(std::find_if(st.temp_ev.begin(), st.temp_ev.end(), [](const Event& te) {
            return te.idx == event::Type::MouseMove;
        }) == st.temp_ev.end());
    st.mouse_pos = plug::get().mouse_from_window(x, y);
    return true;
}

void state::fill_kbd_state(unsigned char* data) {
    for (auto& val : st.prev_input)
        data[val] = 1;
}

void state::on_mode_switch() {
    auto& cfg = conf::get();
    cur_holding.clear();
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
    ImGui::Text("[RE%s]%s%s", cfg.is_replay ? "PLAY" : "CORD",
                video::is_recording() ? " [VIDEO]" : "", audio::is_recording() ? " [AUDIO]" : "");
    ImGui::Text("Frames: %i / %i", st.frames, st.total);
    if (!cfg.fast_forward) {
        ImGui::Text("Scene: %i", st.scene);
        ImGui::Text("Re-records: %llu", st.rerec_count);
    }
#ifdef _DEBUG
    if (!cfg.fast_forward) {
        ImGui::Text("Replay index: %i", repl_index);
        ImGui::Text("Event count: %i", static_cast<int>(st.ev.size()));
    }
#endif
    if (!cfg.fast_forward)
        ImGui::Text("Temp event count: %i", static_cast<int>(st.temp_ev.size()));
    ImGui::Text("Message: %s", last_msg.c_str());
    if (!cfg.fast_forward) {
        auto m_pos = get_tas_mouse_pos();
        auto win_pos = plug::get().mouse_to_window(m_pos.first, m_pos.second);
        ImGui::Text("Window mouse%s: %i, %i", get_tas_mouse_down(VK_LBUTTON) ? " [DOWN]" : "",
                    win_pos.first, win_pos.second);
        std::string keys_str;
        for (auto& vk : st.prev_input) {
            auto opt = input::vk_to_string(vk);
            ASS(opt.has_value());
            keys_str += std::string(opt.value());
            keys_str += ", ";
        }
        ImGui::Text("Keys: %s",
                    keys_str.empty() ? "" : keys_str.substr(0, keys_str.size() - 2).c_str());
    }
}

void state::clear_temp_events() {
    st.temp_ev.clear();
    last_msg = "Temp events queue cleared";
}

void state::reset_game() {
    st.prev_input.clear();
    st.ev.clear();
    st.frames = 0;
    st.total = 0;
    st.prev_input.clear();
    st.mouse_pos.first = st.mouse_pos.second = -1.f;
    msgbox_buf.clear();
    cur_holding.clear();
    repl_index = 0;
    void* pState = plug::get().get_prop(plug::PtrProp::PState);
    short* ptr =
        reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameTask, pState));
    *ptr = 4;
    ptr = reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameData, pState));
    *ptr = 0;
    files::clear_fs();
    audio::reinit_capture();
}
