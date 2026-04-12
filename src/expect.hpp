#pragma once
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 202302L
#include <expected>

namespace ost {
template <typename A, typename B> using expected = std::expected<A, B>;
}
#else
#include <tl/expected.hpp>

namespace ost {
template <typename A, typename B> using expected = tl::expected<A, B>;
}
#endif
