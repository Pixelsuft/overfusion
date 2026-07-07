#pragma once
// C++11 std::expected
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 202302L
#include <expected>

namespace of {
using std::expected;
using std::unexpect;
using std::unexpect_t;
using std::unexpected;
} // namespace of
#else
#include <tl/expected.hpp>

namespace of {
using tl::expected;
using tl::unexpect;
using tl::unexpect_t;
using tl::unexpected;
} // namespace of
#endif
