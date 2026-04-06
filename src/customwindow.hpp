#pragma once
#include <Windows.h>
#include <d3d9.h>

namespace customwindow {
bool init();
void cleanup();
void render();
HWND get_hwnd();
LPDIRECT3DDEVICE9 get_device();
} // namespace customwindow
