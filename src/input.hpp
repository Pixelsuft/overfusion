#pragma once

namespace input {
void init();
void handle_input(int vk, bool pressed);
void handle_input_real(int vk, bool pressed);
void process_update();
} // namespace input
