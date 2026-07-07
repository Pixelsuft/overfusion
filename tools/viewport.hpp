#pragma once
#include "../src/sv.hpp"

namespace viewport {
void after_dll_load(of::string_view fn, void* mod);
void* after_proc_get(void* module, const char* proc, void* ret);
void draw_menu();
} // namespace viewport
