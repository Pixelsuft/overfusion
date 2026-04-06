#pragma once
#include <Windows.h>
#include <d3d9.h>

namespace ui {
extern bool processing;

void init();
bool init_imgui_context();
bool init_imgui_platform(HWND hwnd, LPDIRECT3DDEVICE9 device);
void draw();
} // namespace ui
