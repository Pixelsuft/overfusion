#pragma once
#include "expect.hpp"
#include "ofs.hpp"
#include "sv.hpp"
#include <cstdint>

namespace state {
enum class TimeOffset { None, System, Local, Startup, Reminder };

void init();
bool invalidate_process(ost::string_view text);
void early_update();
void before_update();
void after_update();
ost::expected<void, std::string> save_game(ofs::File& file);
ost::expected<void, std::string> load_game(ofs::File& file);
uint64_t get_time(TimeOffset offset);
int64_t get_utc_offset();
void set_temp_time_offset(int ms);
bool get_key_state(int vk);
void set_key_down(int vk, bool down);
void fill_kbd_state(unsigned char* data);
void draw_info();
void save_state(int slot);
void load_state(int slot);
} // namespace state
