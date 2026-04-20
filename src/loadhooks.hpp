#pragma once

namespace loadhook {
void init();
void* get_func_address(void* handle, const char* func_name);
}
