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

    Config();
    void read();
};

void init();
Config& get();
} // namespace config
