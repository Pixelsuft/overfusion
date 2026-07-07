#pragma once
// C++11 std::optional
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 201703L
#include <optional>

namespace of {
using std::nullopt;
using std::nullopt_t;
using std::optional;
} // namespace of
#else
#include <tl/optional.hpp>

namespace of {
using tl::nullopt;
using tl::nullopt_t;
using tl::optional;
} // namespace of
#endif
