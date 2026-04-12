#pragma once
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 202302L
#include <expected>

namespace ost {
template <typename T, typename E> using expected = std::expected<T, E>;
using unexpect_t = std::unexpect_t;
inline constexpr unexpect_t unexpect{};
} // namespace ost
#else
#include <tl/expected.hpp>

namespace ost {
template <typename T, typename E> using expected = tl::expected<T, E>;
using unexpect_t = tl::unexpect_t;
static constexpr unexpect_t unexpect = tl::unexpect;
} // namespace ost
#endif
