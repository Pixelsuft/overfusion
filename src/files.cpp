#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "files.hpp"
#include "ass.hpp"
#include "lock.hpp"
#include "mem.hpp"
#include "uconv.hpp"
#include <Windows.h>
#include <map>
#include <spdlog/spdlog.h>

using std::string;

struct FileData {
    char* data;
    size_t size;
    int security;
};

struct FileHandle {
    char* data;
    int pos;
    int mode;
};

static lock::CriticalSection cs;
static string real_cwd;
static string temp_path;
static std::map<std::string, FileData> file_map;

static HANDLE(WINAPI* CreateFileWO)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                    DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                    HANDLE hTemplateFile);
static std::string normalize_path(ost::string_view path_view) {
    // TODO: actually return unique fp but the same for each file
    return string(path_view);
}

ost::string_view filehooks::get_cwd() { return real_cwd; }

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

BOOL(WINAPI* SetCurrentDirectoryWO)(LPCWSTR lpPathName);
BOOL WINAPI SetCurrentDirectoryWH(LPCWSTR lpPathName) {
    spdlog::info("SetCurrentDirectoryW: {}", uconv::from_utf16(lpPathName));
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

static HANDLE(WINAPI* CreateFileAO)(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                    DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                    HANDLE hTemplateFile);
static HANDLE WINAPI CreateFileAH(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                  HANDLE hTemplateFile) {
    ASS(lpFileName != nullptr);
    lock::CSLock mylock(cs);
    // spdlog::debug("CreateFileA: {}", lpFileName);
    return CreateFileAO(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HANDLE WINAPI CreateFileWH(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                  HANDLE hTemplateFile) {
    ASS(lpFileName != nullptr);
    lock::CSLock mylock(cs);
    auto fp = normalize_path(uconv::from_utf16(lpFileName));
    spdlog::debug("CreateFileW: {}", fp);
    return CreateFileWO(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HFILE(WINAPI* OpenFileO)(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle);
static HFILE WINAPI OpenFileH(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle) {
    lock::CSLock mylock(cs);
    spdlog::info("OpenFile: {}", lpFileName);
    return OpenFileO(lpFileName, lpReOpenBuff, uStyle);
}

static BOOL(WINAPI* CloseHandleO)(HANDLE hObject);
static BOOL WINAPI CloseHandleH(HANDLE hObject) {
    lock::CSLock mylock(cs);
    return CloseHandleO(hObject);
}

static BOOL(WINAPI* WritePrivateProfileStringAO)(LPCSTR lpAppName, LPCSTR lpKeyName,
                                                 LPCSTR lpString, LPCSTR lpFileName);
static BOOL WINAPI WritePrivateProfileStringAH(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString,
                                               LPCSTR lpFileName) {
    lock::CSLock mylock(cs);
    spdlog::debug("WritePrivateProfileStringA: File={}, Section=[{}], Key={}, Value={}",
                  lpFileName ? lpFileName : "(null)", lpAppName ? lpAppName : "",
                  lpKeyName ? lpKeyName : "", lpString ? lpString : "");

    return WritePrivateProfileStringAO(lpAppName, lpKeyName, lpString, lpFileName);
}

static BOOL(WINAPI* WritePrivateProfileStringWO)(LPCWSTR lpAppName, LPCWSTR lpKeyName,
                                                 LPCWSTR lpString, LPCWSTR lpFileName);
static BOOL WINAPI WritePrivateProfileStringWH(LPCWSTR lpAppName, LPCWSTR lpKeyName,
                                               LPCWSTR lpString, LPCWSTR lpFileName) {
    lock::CSLock mylock(cs);
    spdlog::debug("WritePrivateProfileStringW: File={}, Section=[{}], Key={}, Value={}",
                  lpFileName ? uconv::from_utf16(lpFileName) : "(null)",
                  lpAppName ? uconv::from_utf16(lpAppName) : "",
                  lpKeyName ? uconv::from_utf16(lpKeyName) : "",
                  lpString ? uconv::from_utf16(lpString) : "");

    return WritePrivateProfileStringWO(lpAppName, lpKeyName, lpString, lpFileName);
}

static DWORD(WINAPI* GetPrivateProfileStringAO)(LPCSTR lpAppName, LPCSTR lpKeyName,
                                                LPCSTR lpDefault, LPSTR lpReturnedString,
                                                DWORD nSize, LPCSTR lpFileName);
static DWORD WINAPI GetPrivateProfileStringAH(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault,
                                              LPSTR lpReturnedString, DWORD nSize,
                                              LPCSTR lpFileName) {
    lock::CSLock mylock(cs);
    auto ret = GetPrivateProfileStringAO(lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize,
                                         lpFileName);
    spdlog::debug("GetPrivateProfileStringA: File={}, Section=[{}], Key={}, Result={}",
                  lpFileName ? lpFileName : "(null)", lpAppName, lpKeyName, lpReturnedString);
    return ret;
}

static DWORD(WINAPI* GetPrivateProfileStringWO)(LPCWSTR lpAppName, LPCWSTR lpKeyName,
                                                LPCWSTR lpDefault, LPWSTR lpReturnedString,
                                                DWORD nSize, LPCWSTR lpFileName);
static DWORD WINAPI GetPrivateProfileStringWH(LPCWSTR lpAppName, LPCWSTR lpKeyName,
                                              LPCWSTR lpDefault, LPWSTR lpReturnedString,
                                              DWORD nSize, LPCWSTR lpFileName) {
    lock::CSLock mylock(cs);
    auto ret = GetPrivateProfileStringWO(lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize,
                                         lpFileName);
    spdlog::debug("GetPrivateProfileStringW: File={}, Section=[{}], Key={}, Result={}",
                  lpFileName ? uconv::from_utf16(lpFileName) : "(null)",
                  uconv::from_utf16(lpAppName), uconv::from_utf16(lpKeyName),
                  uconv::from_utf16(lpReturnedString));
    return ret;
}

void filehooks::init() {
    // HOOK_AUTO("kernel32.dll", SetCurrentDirectoryW);
    HOOK_STR_ONLY("kernel32.dll", GetTempPath);
    HOOK_STR_AUTO("kernel32.dll", CreateFile);
    // HOOK_STR_AUTO("kernel32.dll", WritePrivateProfileString);
    // HOOK_STR_AUTO("kernel32.dll", GetPrivateProfileString);
    // HOOK_AUTO("kernel32.dll", OpenFile);
    HOOK_AUTO("kernel32.dll", CloseHandle);
}
