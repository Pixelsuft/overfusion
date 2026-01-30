#pragma once
#include <string_view>

namespace filehooks {
void pre_init();
void init();
std::string_view get_cwd();
}
