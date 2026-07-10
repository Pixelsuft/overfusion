#pragma once
#include "sv.hpp"

namespace mmfhooks {
void init(void* mod);
void* cctrans_get_proc(of::string_view proc, void* ret);
}
