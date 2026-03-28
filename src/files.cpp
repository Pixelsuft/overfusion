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
#include <imgui.h>
#include <map>
#include <spdlog/spdlog.h>
#undef min
#undef max

// TODO: implement other filesystem functions

using ost::string_view;
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

HANDLE(WINAPI* CreateFileWO)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileW;
HANDLE(WINAPI* CreateFileAO)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) = CreateFileA;
BOOL(WINAPI* CloseHandleO)(HANDLE hObject) = CloseHandle;

static std::string normalize_path(string_view path_view) {
    if (path_view.empty())
        return "";
    string ret(path_view);
    // TODO:
    // - UTF-8 support
    // - support for '..' and '.'
    std::transform(ret.begin(), ret.end(), ret.begin(),
                   [](unsigned char c) { return c != '/' ? std::tolower(c) : '\\'; });
    // std::replace(ret.begin(), ret.end(), '/', '\\');
    if (ret[0] == '\\')
        ret.insert(0, string() + static_cast<char>(std::tolower(real_cwd[0])) + ':');
    return ret;
}

string_view files::get_cwd() { return real_cwd; }

void files::pre_init() {
    [] {
        wchar_t buffer[MAX_PATH];
        auto len_ret = GetCurrentDirectoryW(MAX_PATH, buffer);
        ASS(len_ret > 0 && len_ret < MAX_PATH);
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

static bool try_read_file(FileData& ret, std::string_view path) {
    ofs::File file(path, 0);
    if (!file.is_open())
        return false;
    auto temp_size = file.size();
    if (temp_size < 0) {
        spdlog::warn("Failed to get file size on disk: {}", path);
        return false;
    }
    ret.size = static_cast<size_t>(temp_size);
    ret.data = ret.data ? std::realloc(ret.data, ret.size) : std::malloc(ret.size);
    ASS(ret.data != nullptr);
    if (file.read(ret.data, ret.size))
        return true;
    spdlog::warn("Failed to read file from disk: {}", path);
    std::free(ret.data);
    ret.data = nullptr;
    return false;
}

static bool create_file_data(FileData& ret, std::string_view path, DWORD dwCreationDisposition,
                             bool exists) {
    // TODO: more accurate CREATE_NEW, TRUNCATE_EXISTING, OPEN_EXISTING
    switch (dwCreationDisposition) {
    case CREATE_ALWAYS:
    case CREATE_NEW:
    case TRUNCATE_EXISTING:
        if (ret.data && !ret.allow_write) {
            SetLastError(ERROR_ACCESS_DENIED);
            return false;
        }
        if (ret.data)
            std::free(ret.data);
        ret.data = std::malloc(1);
        ret.size = 0;
        break;
    case OPEN_ALWAYS:
    case OPEN_EXISTING:
        if (exists && !ret.data) {
            ret.data = std::malloc(1);
            ret.size = 0;
            break;
        }
        if (!ret.data && !try_read_file(ret, path)) {
            ret.size = 0;
            SetLastError(ERROR_ACCESS_DENIED);
            return false;
        }
        break;
    default:
        ret.data = nullptr;
        ret.size = 0;
        ASS(false);
        return false;
    }
    ASS(ret.data != nullptr);
    return true;
}

static bool is_allowed_file(std::string_view path) {
    return true;
    return !path.ends_with(".mfx") && !path.ends_with(".mvx") && !path.ends_with(".dll") &&
           !path.ends_with(".sft") && !path.ends_with(".ift") && !path.ends_with(".exe") &&
           !path.ends_with(".bin");
}

static std::optional<void*> handle_file_open(std::string_view path, bool for_read, bool for_write,
                                             DWORD dwCreationDisposition) {
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
        // spdlog::debug("Passing through file opened for reading: {}", path);
        return {};
    }
    if (for_write && it == file_map.end()) {
        file_map[norm_fp] = FileData();
        if (!create_file_data(file_map[norm_fp], norm_fp, dwCreationDisposition, false))
            return INVALID_HANDLE_VALUE;
    } else {
        FileData& data = it->second;
        if (data.refcount != 0) {
            if ((for_read && !data.allow_read) || (for_write && !data.allow_write)) {
                spdlog::warn("Access denied for file: {}", path);
                SetLastError(ERROR_ACCESS_DENIED);
                return INVALID_HANDLE_VALUE;
            }
        }
        if (!create_file_data(data, norm_fp, dwCreationDisposition, true))
            return INVALID_HANDLE_VALUE;
    }
    FileData& dp = file_map[norm_fp];
    dp.allow_read = !for_write;
    dp.allow_write = false;
    auto handle = new FileHandle(dp, for_read, for_write);
    our_handles.push_back(handle);
    // spdlog::debug("Started file emulation: {}", norm_fp);
    return handle;
}

static HANDLE WINAPI CreateFileAH(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                  DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                                  HANDLE hTemplateFile) {
    ASS(lpFileName != nullptr);
    auto temp_ret = handle_file_open(uconv::from_ansi(lpFileName), dwDesiredAccess & GENERIC_READ,
                                     dwDesiredAccess & GENERIC_WRITE, dwCreationDisposition);
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
    auto temp_ret = handle_file_open(uconv::from_utf16(lpFileName), dwDesiredAccess & GENERIC_READ,
                                     dwDesiredAccess & GENERIC_WRITE, dwCreationDisposition);
    if (temp_ret.has_value())
        return reinterpret_cast<HANDLE>(temp_ret.value());
    return CreateFileWO(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static BOOL(WINAPI* ReadFileO)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = ReadFile;
static BOOL WINAPI ReadFileH(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                             LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return ReadFileO(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    auto h = *it;
    if (!h->reading) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    size_t available = h->data.size > h->pos ? h->data.size - h->pos : 0;
    size_t to_read = std::min((size_t)nNumberOfBytesToRead, available);
    if (to_read > 0) {
        memcpy(lpBuffer, (char*)h->data.data + h->pos, to_read);
        h->pos += to_read;
    }
    if (lpNumberOfBytesRead)
        *lpNumberOfBytesRead = (DWORD)to_read;
    return TRUE;
}

static BOOL(WINAPI* WriteFileO)(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED) = WriteFile;
static BOOL WINAPI WriteFileH(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                              LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return WriteFileO(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten,
                          lpOverlapped);
    auto h = *it;
    if (!h->writing) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    size_t needed_size = h->pos + nNumberOfBytesToWrite;
    if (needed_size > h->data.size) {
        void* new_data = std::realloc(h->data.data, needed_size);
        ASS(new_data != nullptr);
        h->data.data = new_data;
        h->data.size = needed_size;
    }
    memcpy((char*)h->data.data + h->pos, lpBuffer, nNumberOfBytesToWrite);
    h->pos += nNumberOfBytesToWrite;
    if (lpNumberOfBytesWritten)
        *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
    return TRUE;
}

static BOOL(WINAPI* ReadFileExO)(HANDLE, LPVOID, DWORD, LPOVERLAPPED,
                                 LPOVERLAPPED_COMPLETION_ROUTINE) = ReadFileEx;
static BOOL WINAPI ReadFileExH(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                               LPOVERLAPPED lpOverlapped,
                               LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return ReadFileExO(hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped,
                           lpCompletionRoutine);
    auto h = *it;
    mylock.unlock();
    if (!h->reading) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    DWORD bytesRead = 0;
    ReadFileH(hFile, lpBuffer, nNumberOfBytesToRead, &bytesRead, nullptr);
    if (lpOverlapped) {
        unsigned long long next_pos = (unsigned long long)h->pos;
        lpOverlapped->Offset = (DWORD)(next_pos & 0xFFFFFFFF);
        lpOverlapped->OffsetHigh = (DWORD)(next_pos >> 32);
        lpOverlapped->Internal = 0;
        lpOverlapped->InternalHigh = bytesRead;
    }
    if (lpCompletionRoutine) {
        lpCompletionRoutine(0, bytesRead, lpOverlapped);
    }
    return TRUE;
}

static BOOL(WINAPI* WriteFileExO)(HANDLE, LPCVOID, DWORD, LPOVERLAPPED,
                                  LPOVERLAPPED_COMPLETION_ROUTINE) = WriteFileEx;
static BOOL WINAPI WriteFileExH(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                                LPOVERLAPPED lpOverlapped,
                                LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return WriteFileExO(hFile, lpBuffer, nNumberOfBytesToWrite, lpOverlapped,
                            lpCompletionRoutine);
    auto h = *it;
    mylock.unlock();
    if (!h->writing) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    DWORD bytesWritten = 0;
    WriteFileH(hFile, lpBuffer, nNumberOfBytesToWrite, &bytesWritten, nullptr);
    if (lpOverlapped) {
        unsigned long long next_pos = (unsigned long long)h->pos;
        lpOverlapped->Offset = (DWORD)(next_pos & 0xFFFFFFFF);
        lpOverlapped->OffsetHigh = (DWORD)(next_pos >> 32);
        lpOverlapped->Internal = 0;
        lpOverlapped->InternalHigh = bytesWritten;
    }
    if (lpCompletionRoutine) {
        lpCompletionRoutine(0, bytesWritten, lpOverlapped);
    }
    return TRUE;
}

static DWORD(WINAPI* SetFilePointerO)(HANDLE, LONG, PLONG, DWORD) = SetFilePointer;
static DWORD WINAPI SetFilePointerH(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
                                    DWORD dwMoveMethod) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return SetFilePointerO(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    auto h = *it;
    long long offset = lDistanceToMove;
    if (lpDistanceToMoveHigh)
        offset |= ((long long)*lpDistanceToMoveHigh << 32);
    long long new_pos = h->pos;
    switch (dwMoveMethod) {
    case FILE_BEGIN:
        new_pos = offset;
        break;
    case FILE_CURRENT:
        new_pos += offset;
        break;
    case FILE_END:
        new_pos = (long long)h->data.size + offset;
        break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_SET_FILE_POINTER;
    }
    if (new_pos < 0) {
        SetLastError(ERROR_NEGATIVE_SEEK);
        return INVALID_SET_FILE_POINTER;
    }
    h->pos = (size_t)new_pos;
    if (lpDistanceToMoveHigh)
        *lpDistanceToMoveHigh = (LONG)(new_pos >> 32);
    return (DWORD)(new_pos & 0xFFFFFFFF);
}

static DWORD(WINAPI* GetFileSizeO)(HANDLE, LPDWORD) = GetFileSize;
static DWORD WINAPI GetFileSizeH(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return GetFileSizeO(hFile, lpFileSizeHigh);
    auto h = *it;
    unsigned long long full_size = (unsigned long long)h->data.size;
    if (lpFileSizeHigh)
        *lpFileSizeHigh = (DWORD)(full_size >> 32);
    DWORD low_part = (DWORD)(full_size & 0xFFFFFFFF);
    if (low_part == INVALID_FILE_SIZE) {
        SetLastError(NO_ERROR);
    }
    return low_part;
}

static BOOL(WINAPI* SetFilePointerExO)(HANDLE, LARGE_INTEGER, PLARGE_INTEGER,
                                       DWORD) = SetFilePointerEx;
static BOOL WINAPI SetFilePointerExH(HANDLE hFile, LARGE_INTEGER liDistanceToMove,
                                     PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return SetFilePointerExO(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    auto h = *it;
    long long offset = liDistanceToMove.QuadPart;
    long long new_pos = 0;
    switch (dwMoveMethod) {
    case FILE_BEGIN:
        new_pos = offset;
        break;
    case FILE_CURRENT:
        new_pos = (long long)h->pos + offset;
        break;
    case FILE_END:
        new_pos = (long long)h->data.size + offset;
        break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (new_pos < 0) {
        SetLastError(ERROR_NEGATIVE_SEEK);
        return FALSE;
    }
    h->pos = (size_t)new_pos;
    if (lpNewFilePointer) {
        lpNewFilePointer->QuadPart = new_pos;
    }
    return TRUE;
}

static BOOL(WINAPI* GetFileSizeExO)(HANDLE, PLARGE_INTEGER) = GetFileSizeEx;
static BOOL WINAPI GetFileSizeExH(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
    lock::CSLock mylock(cs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return GetFileSizeExO(hFile, lpFileSize);
    auto h = *it;
    if (!lpFileSize) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    lpFileSize->QuadPart = (long long)h->data.size;
    return TRUE;
}

static HFILE WINAPI OpenFileH(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle) {
    // TODO: implement
    spdlog::warn("Attempted to use old OpenFile, failing: {}", uconv::from_ansi(lpFileName));
    return HFILE_ERROR;
}

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

void files::init() {
    // HOOK_AUTO("kernel32.dll", SetCurrentDirectoryW);
    HOOK_STR_ONLY("kernel32.dll", GetTempPath);
}

void files::hook_fs() {
    HOOK_STR_AUTO("kernel32.dll", CreateFile);
    HOOK_STR_AUTO("kernel32.dll", WritePrivateProfileString);
    HOOK_STR_AUTO("kernel32.dll", GetPrivateProfileString);
    HOOK_ONLY("kernel32.dll", OpenFile);
    HOOK_AUTO("kernel32.dll", ReadFile);
    HOOK_AUTO("kernel32.dll", ReadFileEx);
    HOOK_AUTO("kernel32.dll", WriteFile);
    HOOK_AUTO("kernel32.dll", WriteFileEx);
    HOOK_AUTO("kernel32.dll", SetFilePointer);
    HOOK_AUTO("kernel32.dll", SetFilePointerEx);
    HOOK_AUTO("kernel32.dll", GetFileSize);
    HOOK_AUTO("kernel32.dll", GetFileSizeEx);
    HOOK_AUTO("kernel32.dll", CloseHandle);
}

void files::draw_ui() {
    ImGui::Text("Opened handles: %i", static_cast<int>(our_handles.size()));
    for (auto& it : file_map)
        ImGui::Text("%s: %i bytes", it.first.c_str(), static_cast<int>(it.second.size));
}
