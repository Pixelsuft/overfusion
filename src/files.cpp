#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "files.hpp"
#include "ass.hpp"
#include "lock.hpp"
#include "mem.hpp"
#include "ofs.hpp"
#include "uconv.hpp"
#include <Windows.h>
#include <algorithm>
#include <map>
#include <spdlog/spdlog.h>

using std::string;

struct FileData {
    void* data;
    size_t size;
    int refcount;
    bool allow_read;
    bool allow_write;

    FileData() : data(nullptr), size(0), refcount(0), allow_read(false), allow_write(false) {}
};

struct FileHandle {
    FileData& data;
    size_t pos;
    bool reading;
    bool writing;

    FileHandle(FileData& _data, bool _allow_read, bool _allow_write)
        : data(_data), reading(_allow_read), writing(_allow_write), pos(0) {
        data.refcount++;
    }
    ~FileHandle() {
        data.refcount--;
        if (data.refcount == 0) {
            data.allow_read = data.allow_write = true;
        }
    }
};

static lock::CriticalSection cs;
static string real_cwd;
static string temp_path;
static std::map<std::string, FileData> file_map;
static std::vector<FileHandle*> our_handles;

HANDLE(WINAPI* CreateFileWO)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                             HANDLE) = CreateFileW;

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

static FileData create_file_data(std::string_view path, int create_mode) {
    // TODO: respect create_mode
    FileData ret;
    ret.data = std::malloc(1);
    ASS(ret.data != nullptr);
    ret.allow_read = ret.allow_write = true;
    ret.size = 0;
    return ret;
}

static bool is_allowed_file(std::string_view path) {
    // spdlog::debug("file: {}", path);
    return path.ends_with("SaveFile1.ini");
}

static std::optional<void*> handle_file_open(std::string_view path, bool for_read, bool for_write,
                                             int create_mode) {
    string norm_fp = normalize_path(path);
    if (!is_allowed_file(norm_fp))
        return {};
    if (!for_read && !for_write) {
        spdlog::warn("Attempted to open a file without permissions (WTF?), failing: {}", path);
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    lock::CSLock mylock(cs);
    auto it = file_map.find(norm_fp);
    if (!for_write && it == file_map.end()) {
        spdlog::debug("Passing through file opened for reading: {}", path);
        return {};
    }
    if (for_write && it == file_map.end()) {
        file_map[norm_fp] = create_file_data(path, create_mode);
    } else {
        FileData& data = it->second;
        if (data.refcount != 0) {
            if ((for_read && !data.allow_read) || (for_write && !data.allow_write)) {
                spdlog::warn("Access denied for file: {}", path);
                SetLastError(ERROR_ACCESS_DENIED);
                return INVALID_HANDLE_VALUE;
            }
        }
    }
    FileData& dp = file_map[norm_fp];
    dp.allow_read = !for_write;
    dp.allow_write = false;
    auto handle = new FileHandle(dp, for_read, for_write);
    our_handles.push_back(handle);
    spdlog::warn("TODO: {}", norm_fp);
    return handle;
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
    // TODO: proper create mode
    auto temp_ret = handle_file_open(uconv::from_ansi(lpFileName), dwDesiredAccess & GENERIC_READ,
                                     dwDesiredAccess & GENERIC_WRITE, 0);
    if (temp_ret.has_value())
        return reinterpret_cast<HANDLE>(temp_ret.value());
    return CreateFileAO(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HANDLE WINAPI CreateFileWH(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                  HANDLE hTemplateFile) {
    ASS(lpFileName != nullptr);
    // TODO: proper create mode
    auto temp_ret = handle_file_open(uconv::from_utf16(lpFileName), dwDesiredAccess & GENERIC_READ,
                                     dwDesiredAccess & GENERIC_WRITE, 0);
    if (temp_ret.has_value())
        return reinterpret_cast<HANDLE>(temp_ret.value());
    return CreateFileWO(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HFILE WINAPI OpenFileH(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle) {
    spdlog::warn("Attempted to use old OpenFile, failing: {}", uconv::from_ansi(lpFileName));
    return HFILE_ERROR;
}

static BOOL(WINAPI* CloseHandleO)(HANDLE hObject);
static BOOL WINAPI CloseHandleH(HANDLE hObject) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hObject);
    if (it == our_handles.end())
        return CloseHandleO(hObject);
    delete *it;
    our_handles.erase(it);
    return TRUE;
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
    HOOK_ONLY("kernel32.dll", OpenFile);
    HOOK_AUTO("kernel32.dll", CloseHandle);
}
