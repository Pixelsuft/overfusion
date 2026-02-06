#pragma once
#ifdef _DEBUG
#include <cassert>

#define ASS(expr) assert(expr)
#else
#define ASS(expr) __assume(expr)
#endif
