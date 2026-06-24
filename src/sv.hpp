#pragma once
// C++11 std::string_view
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 201703L
#include <string_view>

namespace ost {
using std::string_view;
}
#else
#include <bpstd/string_view.hpp>

namespace ost {
using bpstd::string_view;
}
#endif
