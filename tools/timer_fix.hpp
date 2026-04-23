#pragma once
#include "../src/expect.hpp"
#include <string>
#include <vector>

namespace timer_fix {
ost::expected<void, std::string> save(std::vector<int>& data);
ost::expected<void, std::string> load(std::vector<int> data);
} // namespace timer_fix
