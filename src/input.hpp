#pragma once
#include "sv.hpp"

namespace input {
void init();
void handle_input(int vk, bool pressed);
void handle_input_real(int vk, bool pressed);
void process_update();
void sim_key_event(int vk, bool down);
void sim_mouse_event(int vk, bool down);
void sim_mouse_move(int x, int y);
int vk_from_string(ost::string_view s);
} // namespace input
