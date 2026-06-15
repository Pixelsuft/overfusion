#pragma once

namespace ui {
void init();
void* init_imgui_context();
bool init_imgui_platform(void* hwnd, void* device);
void draw(bool force_custom_menu);
bool is_processing();
void set_processing(bool enabled);
} // namespace ui
