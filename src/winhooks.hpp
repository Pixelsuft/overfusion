#pragma once
#include <utility>
#include "sv.hpp"

namespace winhooks {
void fix_win32_theme(void* hwnd);
void init_win32_theme();
void init();
void after_ui_init();
std::pair<int, int> get_size();
void display_ensure_fail(ost::string_view text);
} // namespace winhooks
