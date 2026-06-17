#pragma once
#include "ofs.hpp"
#include "sv.hpp"

namespace files {
void pre_init();
void init();
void clear_fs();
void hook_fs();
void draw_ui();
// TODO: move to ofs (these 3)?
ost::string_view get_cwd();
bool save_fs(ofs::File& file);
bool load_fs(ofs::File& file);
} // namespace files
