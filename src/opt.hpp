#pragma once
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 201703L
#include <optional>

namespace ost {
template <typename T> using optional = std::optional<T>;
}
#else
#include <bpstd/optional.hpp>

namespace ost {
template <typename T> using optional = bpstd::optional<T>;
}
#endif
