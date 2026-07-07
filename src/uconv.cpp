#define WIN32_LEAN_AND_MEAN
#include "uconv.hpp"
#include <Windows.h>
#include <vector>

std::string uconv::from_utf16(const wchar_t* input) {
    if (!input || input[0] == L'\0')
        return "";

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 1)
        return "";

    std::string result;
    result.resize(size_needed - 1);
    if (!WideCharToMultiByte(CP_UTF8, 0, input, size_needed, &result[0], size_needed, nullptr,
                             nullptr))
        return "";

    return result;
}

std::string uconv::from_ansi(const char* input) {
    if (!input)
        return "";

    int wide_size = MultiByteToWideChar(CP_ACP, 0, input, -1, nullptr, 0);
    if (wide_size <= 0)
        return "";

    std::vector<wchar_t> wide_str(wide_size);
    if (!MultiByteToWideChar(CP_ACP, 0, input, wide_size, wide_str.data(), wide_size))
        return "";

    return from_utf16(wide_str.data());
}

wchar_t* uconv::to_utf16(of::string_view input) {
    if (input.empty()) {
        auto ret = reinterpret_cast<wchar_t*>(std::malloc(2));
        if (ret)
            *ret = L'\0';
        return ret;
    }
    int wide_size =
        MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (wide_size < 0)
        return nullptr;

    wchar_t* buffer = reinterpret_cast<wchar_t*>(std::malloc((wide_size + 1) * sizeof(wchar_t)));
    if (!buffer)
        return nullptr;

    if (!MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), buffer,
                             wide_size)) {
        std::free(buffer);
        return nullptr;
    }

    buffer[wide_size] = L'\0';
    return buffer;
}

char* uconv::to_ansi(of::string_view input) {
    if (input.empty()) {
        auto ret = reinterpret_cast<char*>(std::malloc(1));
        if (ret)
            *ret = '\0';
        return ret;
    }
    int wide_size =
        MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (wide_size < 0)
        return nullptr;

    std::vector<wchar_t> wide_buf(wide_size);
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), wide_buf.data(),
                        wide_size);

    int ansi_size =
        WideCharToMultiByte(CP_ACP, 0, wide_buf.data(), wide_size, nullptr, 0, nullptr, nullptr);
    if (ansi_size < 0)
        return nullptr;

    char* buffer = reinterpret_cast<char*>(std::malloc((ansi_size + 1) * sizeof(char)));
    if (!buffer)
        return nullptr;

    if (!WideCharToMultiByte(CP_ACP, 0, wide_buf.data(), wide_size, buffer, ansi_size, nullptr,
                             nullptr)) {
        std::free(buffer);
        return nullptr;
    }

    buffer[ansi_size] = '\0';
    return buffer;
}
