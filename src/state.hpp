#pragma once
#include "ofs.hpp"
#include <cstdint>

namespace state {
    enum class TimeOffset {
        None,
        System,
        Local,
        Startup,
        Reminder
    };

    void init();
    void invalidate_process();
    void early_update();
    void before_update();
    void after_update();
    bool save_game(ofs::File& file);
    bool load_game(ofs::File& file);
    uint64_t get_time(TimeOffset offset);
    int64_t get_utc_offset();
    bool get_key_state(int vk);
    void set_key_down(int vk, bool down);
    void fill_kbd_state(unsigned char* data);
    void draw_info();
    void save_state(int slot);
    void load_state(int slot);
}
