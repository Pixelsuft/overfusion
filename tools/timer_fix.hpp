#pragma once
#include "../src/expect.hpp"
#include "../src/mypair.hpp"
#include <string>
#include <vector>

namespace timer_fix {
ost::expected<void, std::string> save(std::vector<IntPair>& data);
ost::expected<void, std::string> load(std::vector<IntPair> data);
} // namespace timer_fix
