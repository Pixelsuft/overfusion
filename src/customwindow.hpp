#pragma once
#include <Windows.h>
#include <d3d9.h>

namespace customwindow {
bool init();
void cleanup();
void render();
void update_menu_show();
} // namespace customwindow
