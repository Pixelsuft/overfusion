#pragma once
#include <vector>

namespace conf {
enum class Task {
    None = 0,
    SaveState,
    LoadState,
    Advance,
    Play,
    FastForward,
    Map,
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
    int fps;
    bool show_info;
    bool show_menu;
    bool is_replay;
    bool is_paused;
    bool need_advance;
    bool fast_forward;
    bool emulate_user_timers;

    Config();
    void read();
};

void init();
Config& get();
} // namespace config
