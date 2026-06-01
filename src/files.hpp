#pragma once
#include "sv.hpp"
#include "ofs.hpp"

namespace files {
void pre_init();
void init();
void clear_fs();
void hook_fs();
void draw_ui();
ost::string_view get_cwd();
bool save_fs(ofs::File& file);
bool load_fs(ofs::File& file);
} // namespace files
