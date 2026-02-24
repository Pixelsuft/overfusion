#pragma once
#ifdef _DEBUG
#include <cassert>

// TODO: actually difference assert and assume
#define ASS(expr) assert(expr)
#else
#define ASS(expr) __assume(expr)
#endif
