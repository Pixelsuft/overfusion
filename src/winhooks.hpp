#pragma once
#include <utility>

namespace winhooks {
void fix_win32_theme(void* hwnd);
void init();
void after_ui_init();
std::pair<int, int> get_size();
} // namespace winhooks
