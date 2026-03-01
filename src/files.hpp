#pragma once
#include "sv.hpp"

namespace filehooks {
void pre_init();
void init();
ost::string_view get_cwd();
}
