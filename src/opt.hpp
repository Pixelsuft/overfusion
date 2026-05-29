#pragma once
// C++11 std::optional
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 201703L
#include <optional>

namespace ost {
template <typename T> using optional = std::optional<T>;
using nullopt_t = std::nullopt_t;
inline constexpr nullopt_t nullopt = std::nullopt;
} // namespace ost
#else
#include <tl/optional.hpp>

namespace ost {
template <typename T> using optional = tl::optional<T>;
using nullopt_t = tl::nullopt_t;
static constexpr nullopt_t nullopt = tl::nullopt;
} // namespace ost
#endif
