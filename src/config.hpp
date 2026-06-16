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
    Menu,
    ReplayMode,
    ResetGame
};

struct Bind {
    std::vector<int> mods;
    int key;
    int extra;
    Task task;
};

enum class RenderType { None, GDI, DDRAW, D3D8, D3D9 };

class Config {
public:
    std::vector<Bind> binds;
    std::string cmdline_append;
    std::string project_name;
    std::string ffmpeg_cmdline;
    std::pair<int, int> forced_res;
    uint64_t system_offset;
    uint64_t local_offset;
    uint64_t startup_offset;
    void* pUpdateGameFrame;
    void* pRenderFrame;
    void* pProcessTransition;
    void* pRenderTransition;
    size_t tm_fix_event_entry_offset;
    size_t tm_fix_event_entry_type_offset;
    float speed;
    float font_scale;
    int fps;
    int delta_multiplier;
    RenderType render_type;
    bool show_info;
    bool show_menu;
    bool is_replay;
    bool is_paused;
    bool need_advance;
    bool fast_forward;
    bool emulate_user_timers;
    bool emulate_mm_timers;
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
    bool allow_direct_capture;
    bool allow_audio_hook;
    bool disable_audio;
    bool record_audio;
    bool support_audio_panning;
    bool need_key_message;
    bool need_mouse_move_message;
    bool no_mouse_manipulation;
    bool draw_cursor;
    bool pixel_filter;
    bool ui_pixel_filter;
    bool save_vfs;
    bool disable_dark_mode_support;
    bool force_custom_window;
    bool wait_for_debugger;
    bool disable_fullscreen;
    bool disable_perspective;
    bool disable_viewport;
    bool pause_on_scene_switch;

    Config();
    void read();
};

void init();
Config& get();
} // namespace conf
