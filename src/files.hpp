#pragma once
#include "ofs.hpp"

namespace files {
void pre_init();
void init();
void clear_fs();
void hook_fs();
void draw_ui();
bool save_fs(ofs::File& file);
bool load_fs(ofs::File& file);
} // namespace files
