#pragma once
#include <string>
#include <vector>

namespace conf {
enum class Task {
    SaveState,
    LoadState,
    Advance,
    Play,
    FastForward,
    Map,
    MouseDown,
    MouseMove,
    Menu
};

struct Bind {
    std::vector<int> mods;
    int key;
    int extra;
    Task task;
};

class Config {
public:
    std::vector<Bind> binds;
    std::string cmdline_append;
    std::string project_name;
    std::string ffmpeg_cmdline;
    uint64_t system_offset;
    uint64_t local_offset;
    uint64_t startup_offset;
    void* pUpdateGameFrame;
    void* pRenderFrame;
    void* pProcessTransition;
    void* pRenderTransition;
    size_t tm_fix_event_entry_offset;
    size_t tm_fix_event_entry_type_offset;
    int fps;
    bool show_info;
    bool show_menu;
    bool is_replay;
    bool is_paused;
    bool need_advance;
    bool fast_forward;
    bool emulate_user_timers;
    bool emulate_mm_timers;
    bool is_unicode;
    bool custom_window;
    bool virtual_fs;
    bool disable_threads;
    bool delay_thread_hook;
    bool no_ini_hooks;
    bool boxed_mode;
    bool reset_on_replay;
    bool save_game_state;
    bool disable_app_menu;
    bool allow_timers_fix;
    bool allow_d3d9_recording;
    bool allow_audio_hook;
    bool disable_audio;
    bool record_audio;
    bool support_audio_panning;
    bool allow_setting_cursor_pos;

    Config();
    void read();
};

void init();
Config& get();
} // namespace conf
