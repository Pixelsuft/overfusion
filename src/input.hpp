#pragma once
#include "opt.hpp"
#include "sv.hpp"
#include <utility>

namespace input {
void init();
void handle_input(int vk, bool pressed);
void handle_input_real(int vk, bool pressed);
void process_update();
void sim_key_event(int vk, bool down);
void sim_mouse_event(int vk, bool down);
void sim_mouse_move(int x, int y);
std::pair<int, int> get_real_mouse_pos();
ost::optional<int> vk_from_string(ost::string_view s);
ost::optional<ost::string_view> vk_to_string(int vk);
} // namespace input
