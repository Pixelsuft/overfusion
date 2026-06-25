#pragma once
#include "ass.hpp"
#include "ofs.hpp"
#include "sv.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace state {
enum class TimeOffset { None, System, Local, Startup, Reminder };

void init();
bool invalidate_process(ost::string_view text);
bool is_save_handle(void* handle);
void early_update();
bool before_update(bool is_transitioning);
void after_update();
void on_mode_switch();
uint64_t get_time(TimeOffset offset);
int64_t get_utc_offset();
void set_temp_time_offset(int ms);
int get_frame_counter();
bool get_key_state(int vk);
void set_key_down(int vk, bool down);
void add_mouse_toggle(int vk);
void add_mouse_move();
void fill_kbd_state(unsigned char* data);
void draw_info();
void save_state(int slot);
void load_state(int slot);
void reset_game();
void clear_temp_events();
void set_last_msg(ost::string_view msg);
void export_replay(ost::string_view fn);
void import_replay(ost::string_view fn);
std::pair<int, int> get_mouse_pos();
bool get_tas_mouse_down(int vk);
std::pair<float, float> get_tas_mouse_pos();
bool set_win_mouse_pos(int x, int y);
int process_message_box(ost::string_view text, ost::string_view caption, unsigned int uType);
void remember_message_box(int choice);
int fetch_random_number(int range, int value);
void draw_random_tab();

// TODO: maybe to ofs?
template <typename T> static void write_bin(ofs::File& file, const std::vector<T>& data) {
    size_t size = data.size();
    auto ret = file.write(&size, sizeof(size_t));
    ENSURE(ret);
    if (size == 0)
        return;
    // Everything is trivial, no problems, ok?
    ret = file.write(data.data(), size * sizeof(T));
    ENSURE(ret);
}

static void write_bin(ofs::File& file, const std::string& data) {
    size_t size = data.size();
    auto ret = file.write(&size, sizeof(size_t));
    ENSURE(ret);
    if (size == 0)
        return;
    // Everything is trivial, no problems, ok?
    ret = file.write(data.data(), size);
    ENSURE(ret);
}

template <typename A, typename B>
static void write_bin(ofs::File& file, const std::pair<A, B>& val) {
    auto ret = file.write(&val.first, sizeof(A));
    ENSURE(ret);
    ret = file.write(&val.second, sizeof(B));
    ENSURE(ret);
}

template <typename T> static void write_bin(ofs::File& file, const T& val) {
    auto ret = file.write(&val, sizeof(T));
    ENSURE(ret);
}

template <typename T> static void load_bin(ofs::File& file, std::vector<T>& data) {
    size_t size;
    auto ret = file.read(&size, sizeof(size_t));
    ENSURE(ret);
    if (size == 0) {
        data.clear();
        return;
    }
    data.resize(size);
    ret = file.read(&data[0], size * sizeof(T));
    ENSURE(ret);
}

static void load_bin(ofs::File& file, std::string& data) {
    size_t size;
    auto ret = file.read(&size, sizeof(size_t));
    ENSURE(ret);
    if (size == 0) {
        data.clear();
        return;
    }
    data.resize(size);
    ret = file.read(&data[0], size);
    ENSURE(ret);
}

template <typename A, typename B> static void load_bin(ofs::File& file, std::pair<A, B>& val) {
    auto ret = file.read(&val.first, sizeof(A));
    ENSURE(ret);
    ret = file.read(&val.second, sizeof(B));
    ENSURE(ret);
}

template <typename T> static void load_bin(ofs::File& file, T& val) {
    auto ret = file.read(&val, sizeof(T));
    ENSURE(ret);
}
} // namespace state
