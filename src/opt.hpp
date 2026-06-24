#pragma once
// C++11 std::optional
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 201703L
#include <optional>

namespace ost {
using std::nullopt;
using std::nullopt_t;
using std::optional;
} // namespace ost
#else
#include <tl/optional.hpp>

namespace ost {
using tl::nullopt;
using tl::nullopt_t;
using tl::optional;
} // namespace ost
#endif
