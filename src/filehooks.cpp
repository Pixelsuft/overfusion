#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "filehooks.hpp"
#include "ass.hpp"
#include "mem.hpp"
#include "uconv.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using std::string;

static string real_cwd;
static string temp_path;

ost::string_view filehooks::get_cwd() { return real_cwd; }

BOOL(WINAPI* SetCurrentDirectoryWO)(LPCWSTR lpPathName);
BOOL WINAPI SetCurrentDirectoryWH(LPCWSTR lpPathName) {
    spdlog::info("SetCurrentDirectoryWH: {}", uconv::from_utf16(lpPathName));
    return SetCurrentDirectoryWO(lpPathName);
}

DWORD WINAPI GetTempPathAH(DWORD nBufferLength, LPSTR lpBuffer) {
    ASS(nBufferLength > static_cast<DWORD>(temp_path.size()));
    auto temp_str = uconv::to_ansi(temp_path);
    ASS(temp_str != nullptr);
    auto create_ret = CreateDirectoryA(temp_str, nullptr);
    ASS(create_ret || GetLastError() == ERROR_ALREADY_EXISTS);
    strcpy(lpBuffer, temp_str);
    std::free(temp_str);
    return static_cast<DWORD>(temp_path.size());
}

DWORD WINAPI GetTempPathWH(DWORD nBufferLength, LPWSTR lpBuffer) {
    ASS(nBufferLength > static_cast<DWORD>(temp_path.size()));
    auto temp_str = uconv::to_utf16(temp_path);
    ASS(temp_str != nullptr);
    auto create_ret = CreateDirectoryW(temp_str, nullptr);
    ASS(create_ret || GetLastError() == ERROR_ALREADY_EXISTS);
    wcscpy(lpBuffer, temp_str);
    std::free(temp_str);
    return static_cast<DWORD>(temp_path.size());
}

void filehooks::init() {
    // HOOK_AUTO("kernel32.dll", SetCurrentDirectoryW);
    HOOK_ONLY("kernel32.dll", GetTempPathA);
    HOOK_ONLY("kernel32.dll", GetTempPathW);
}

void filehooks::pre_init() {
    [] {
        wchar_t buffer[MAX_PATH];
        auto len_ret = GetCurrentDirectoryW(MAX_PATH, buffer);
        ASS(len_ret > 0);
        buffer[len_ret] = L'\0';
        real_cwd = uconv::from_utf16(buffer);
        temp_path = real_cwd + "\\temp";
    }();
}
