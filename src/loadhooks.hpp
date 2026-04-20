#pragma once

namespace loadhooks {
void init();
void* get_func_address(void* handle, const char* func_name);
} // namespace loadhooks
