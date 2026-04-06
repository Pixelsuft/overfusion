#pragma once
#include "sv.hpp"

namespace files {
void pre_init();
void init();
void hook_fs();
void draw_ui();
ost::string_view get_cwd();
} // namespace files
