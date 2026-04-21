#pragma once
#include "sv.hpp"
#include <cstdint>

namespace state {
enum class TimeOffset { None, System, Local, Startup, Reminder };

void init();
bool invalidate_process(ost::string_view text);
bool is_processing_save(void* handle);
void early_update();
bool before_update();
void after_update();
void on_mode_switch();
uint64_t get_time(TimeOffset offset);
int64_t get_utc_offset();
void set_temp_time_offset(int ms);
bool get_key_state(int vk);
void set_key_down(int vk, bool down);
void fill_kbd_state(unsigned char* data);
void draw_info();
void save_state(int slot);
void load_state(int slot);
void reset_game();
} // namespace state
