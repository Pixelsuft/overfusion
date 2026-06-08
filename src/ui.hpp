#pragma once

namespace ui {
void init();
bool init_imgui_context();
bool init_imgui_platform(void* hwnd, void* device);
void draw();
bool is_processing();
void set_processing(bool enabled);
} // namespace ui
