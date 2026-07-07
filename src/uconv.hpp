#pragma once
#include "sv.hpp"
#include <string>

namespace uconv {
std::string from_utf16(const wchar_t* input);
std::string from_ansi(const char* input);
wchar_t* to_utf16(of::string_view input);
char* to_ansi(of::string_view input);
} // namespace uconv
