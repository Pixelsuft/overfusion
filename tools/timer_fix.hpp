#pragma once
#include "../src/expect.hpp"
#include "../src/ofs.hpp"
#include <vector>

namespace timer_fix {
ost::expected<void, std::string> save(ofs::File& file, std::vector<int>& data);
ost::expected<void, std::string> load(ofs::File& file, std::vector<int> data);
} // namespace timer_fix
