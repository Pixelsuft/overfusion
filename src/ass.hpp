#pragma once
#ifdef _DEBUG
#include <cassert>

// Assume that 'expr' is always fals
#define ASS(expr) assert(expr)
// Run-time assert
#define ENSURE(expr) assert(expr)
#else
#include "winhooks.hpp"

#define _WIDEN_HELPER(x) L##x
#define _WIDEN(x) WIDEN_HELPER(x)
#define __CUSTOM_FILEW__

#define ASS_TO_STRING(x) L#x
#define ASS(expr) __assume(expr)
#define ENSURE(expr)                                                                               \
    do {                                                                                           \
        if (!(expr))                                                                               \
            winhooks::display_ensure_fail(                                                         \
                L"ASSERTION FAILED AT " __CUSTOM_FILEW__ L": " ASS_TO_STRING(expr));               \
    } while (0)
#endif
