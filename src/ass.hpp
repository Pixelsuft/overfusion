#pragma once
#ifdef _DEBUG
#include <cassert>

#define ASS(expr) assert(expr)
#define ENSURE(expr) assert(expr)
#else
#define ASS(expr) __assume(expr)
// TODO
#define ENSURE(expr) if (!(expr)) {}
#endif
