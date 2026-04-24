#pragma once
#include <Windows.h>
#include <d3d9.h>

namespace ui {
void init();
bool init_imgui_context();
bool init_imgui_platform(HWND hwnd, LPDIRECT3DDEVICE9 device);
void draw();
bool is_processing();
void set_processing(bool enabled);
} // namespace ui
