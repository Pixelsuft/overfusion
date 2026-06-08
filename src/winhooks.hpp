#pragma once
#include "sv.hpp"
#include <utility>

namespace winhooks {
void fix_win32_theme(void* hwnd);
void fix_win32_theme_instant(void* hwnd);
void fix_win32_theme_messagebox(void* hwnd);
void fix_win32_set_dark_style(void* hwnd, const wchar_t* style);
void fix_win32_window_bg(void* hwnd);
void init_win32_theme();
void pre_init_win32_theme();
void init();
void after_ui_init();
std::pair<int, int> get_size();
void display_ensure_fail(ost::string_view text);
} // namespace winhooks
