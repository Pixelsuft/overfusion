#pragma once
#include <Windows.h>
#include <d3d9.h>

namespace customwindow {
bool init();
void cleanup();
void render();
void update_menu_show();
void update_info_show();
} // namespace customwindow
