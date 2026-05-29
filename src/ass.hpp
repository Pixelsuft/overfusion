#pragma once
#ifdef _DEBUG
#include <cassert>

// Assume that 'expr' is always fals
#define ASS(expr) assert(expr)
// Run-time assert
#define ENSURE(expr) assert(expr)
#else
#include "winhooks.hpp"

#define ASS_TO_STRING(x) #x
#define ASS(expr) __assume(expr)
#define ENSURE(expr)                                                                               \
    do {                                                                                           \
        if (!(expr))                                                                               \
            winhooks::display_ensure_fail("ASSERTION FAILED AT " __FILE__                          \
                                          ": " ASS_TO_STRING(expr));                               \
    } while (0)
#endif
