#pragma once

namespace input {
void init();
void handle_input(int vk, bool pressed);
void handle_input_real(int vk, bool pressed);
void process_update();
void sim_key_event(int vk, bool down);
void sim_mouse_event(int vk, bool down);
void sim_mouse_move(int x, int y);
} // namespace input
