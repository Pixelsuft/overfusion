#pragma once

namespace gamehooks {
void init();
void hook_update_func(void* ptr);
void set_render_func(void* ptr);
void hook_trans_update_func(void* ptr);
void set_trans_render_func(void* ptr);
} // namespace gamehooks
