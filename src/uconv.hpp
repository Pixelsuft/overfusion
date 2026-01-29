#pragma once
#include <string>
#include <string_view>

namespace uconv {
    std::string from_utf16(const wchar_t* input);
    std::string from_ansi(const char* input);
    wchar_t* to_utf16(std::string_view input);
    char* to_ansi(std::string_view input);
}
