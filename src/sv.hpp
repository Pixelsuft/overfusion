#pragma once
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 201703L
#include <string_view>

namespace ost {
    using string_view = std::string_view;
}
#else
#include <bpstd/string_view.hpp>

namespace ost {
    using string_view = bpstd::string_view;
}
#endif
