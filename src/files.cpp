#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "files.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "lock.hpp"
#include "mem.hpp"
#include "ofs.hpp"
#include "opt.hpp"
#include "state.hpp"
#include "uconv.hpp"
#include <SimpleIni.h>
#include <Windows.h>
#include <algorithm>
#include <fcntl.h>
#include <imgui.h>
#include <map>
#include <spdlog/spdlog.h>
#undef min
#undef max

// TODO: implement other filesystem functions

using ost::optional;
using ost::string_view;
using std::string;

// Our file structure we keep in memory
struct FileData {
    void* data;
    size_t size;
    int refcount;
    bool allow_read;
    bool allow_write;

    FileData() : data(nullptr), size(0), refcount(0), allow_read(false), allow_write(false) {}
};

// File structure we give to the game opening a file
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

static lock::CriticalSection fcs;
static string real_cwd;
static string temp_path;
static std::map<std::string, FileData> file_map;
static std::vector<FileHandle*> our_handles;

HANDLE(WINAPI* CreateFileWO)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                             HANDLE) = CreateFileW;
HANDLE(WINAPI* CreateFileAO)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                             HANDLE) = CreateFileA;
BOOL(WINAPI* CloseHandleO)(HANDLE hObject) = CloseHandle;

static std::string normalize_path(string_view path_view) {
    if (path_view.empty())
        return "";
    string ret(path_view);
    // TODO:
    // - UTF-8 support
    // - support for '..' and '.'
    // - support converting CWD and user folder
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
        ENSURE(len_ret > 0 && len_ret < MAX_PATH);
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
    ENSURE(nBufferLength > static_cast<DWORD>(temp_path.size()));
    auto temp_str = uconv::to_ansi(temp_path);
    ENSURE(temp_str != nullptr);
    auto create_ret = CreateDirectoryA(temp_str, nullptr);
    ENSURE(create_ret || GetLastError() == ERROR_ALREADY_EXISTS);
    strcpy(lpBuffer, temp_str);
    std::free(temp_str);
    return static_cast<DWORD>(temp_path.size());
}

DWORD WINAPI GetTempPathWH(DWORD nBufferLength, LPWSTR lpBuffer) {
    ENSURE(nBufferLength > static_cast<DWORD>(temp_path.size()));
    auto temp_str = uconv::to_utf16(temp_path);
    ENSURE(temp_str != nullptr);
    auto create_ret = CreateDirectoryW(temp_str, nullptr);
    ENSURE(create_ret || GetLastError() == ERROR_ALREADY_EXISTS);
    wcscpy(lpBuffer, temp_str);
    std::free(temp_str);
    return static_cast<DWORD>(temp_path.size());
}

static bool try_read_file(FileData& ret, string_view path) {
    if (ret.data) {
        std::free(ret.data);
        ret.data = nullptr;
    }
    ofs::File file(path, 0);
    if (!file.is_open())
        return false;
    auto temp_size = file.size();
    if (temp_size < 0) {
        spdlog::error("Failed to get file size on disk: {}", path);
        return false;
    }
    ret.size = static_cast<size_t>(temp_size);
    ret.data = ret.data ? std::realloc(ret.data, ret.size) : std::malloc(ret.size);
    ENSURE(ret.data != nullptr);
    if (file.read(ret.data, ret.size))
        return true;
    spdlog::error("Failed to read file from disk: {}", path);
    std::free(ret.data);
    ret.data = nullptr;
    return false;
}

static bool create_file_data(FileData& ret, string_view path, DWORD dwCreationDisposition) {
    bool exists = ret.data != nullptr;
    // spdlog::debug("create_file_data: {} {} {}", path, dwCreationDisposition, exists);
    switch (dwCreationDisposition) {
    case CREATE_ALWAYS:
    case CREATE_NEW:
    case TRUNCATE_EXISTING:
        if (exists && !ret.allow_write) {
            SetLastError(ERROR_ACCESS_DENIED);
            return false;
        }
        if (dwCreationDisposition == CREATE_NEW && (exists || ofs::file_exists(path))) {
            SetLastError(ERROR_ALREADY_EXISTS);
            return false;
        } else if (dwCreationDisposition == TRUNCATE_EXISTING && !exists &&
                   !ofs::file_exists(path)) {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return false;
        }
        if (exists)
            std::free(ret.data);
        ret.data = std::malloc(1);
        ret.size = 0;
        break;
    case OPEN_ALWAYS:
    case OPEN_EXISTING:
        if (dwCreationDisposition == OPEN_EXISTING && !exists && !ofs::file_exists(path)) {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return false;
        }
        if (!exists && !try_read_file(ret, path)) {
            ret.size = 0;
            SetLastError(ERROR_ACCESS_DENIED);
            return false;
        }
        break;
    default:
        if (exists)
            std::free(ret.data);
        ret.data = nullptr;
        ret.size = 0;
        ENSURE(false);
        return false;
    }
    ENSURE(ret.data != nullptr);
    return true;
}

static bool is_allowed_file(string_view path) {
    return true;
    /* return !path.ends_with(".mfx") && !path.ends_with(".mvx") && !path.ends_with(".dll") &&
           !path.ends_with(".sft") && !path.ends_with(".ift") && !path.ends_with(".exe") &&
           !path.ends_with(".bin");*/
}

static optional<void*> handle_file_open(string_view path, bool for_read, bool for_write,
                                        DWORD dwCreationDisposition) {
    // spdlog::debug("open file {}", path);
    string norm_fp = normalize_path(path);
    if (!is_allowed_file(norm_fp))
        return {};
    if (!for_read && !for_write) {
        spdlog::warn("Attempted to open a file without permissions (WTF?), failing: {}", path);
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    lock::CSLock mylock(fcs);
    auto it = file_map.find(norm_fp);
    if (!for_write && it == file_map.end()) {
        // spdlog::debug("Passing through file opened for reading: {}", path);
        return {};
    }
    if (for_write && it == file_map.end()) {
        file_map[norm_fp] = FileData();
        if (!create_file_data(file_map[norm_fp], norm_fp, dwCreationDisposition))
            return INVALID_HANDLE_VALUE;
    } else {
        FileData& data = it->second;
        if (data.refcount != 0) {
            if ((for_read && !data.allow_read) || (for_write && !data.allow_write)) {
                spdlog::warn("Access denied for file: {} (refcount: {})", path, data.refcount);
                SetLastError(ERROR_ACCESS_DENIED);
                return INVALID_HANDLE_VALUE;
            }
        }
        if (!create_file_data(data, norm_fp, dwCreationDisposition))
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
    ENSURE(lpFileName != nullptr);
    auto temp_ret = handle_file_open(uconv::from_ansi(lpFileName), dwDesiredAccess & GENERIC_READ,
                                     dwDesiredAccess & GENERIC_WRITE, dwCreationDisposition);
    if (temp_ret.has_value())
        return reinterpret_cast<HANDLE>(temp_ret.value());
    return CreateFileAO(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI CreateFileWH(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                           LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                           DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    ENSURE(lpFileName != nullptr);
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
    if (state::is_save_handle(hFile))
        return ReadFileO(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    lock::CSLock mylock(fcs);
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
    if (state::is_save_handle(hFile))
        return WriteFileO(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten,
                          lpOverlapped);
    lock::CSLock mylock(fcs);
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
        ENSURE(new_data != nullptr);
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
    if (state::is_save_handle(hFile))
        return ReadFileExO(hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped,
                           lpCompletionRoutine);
    // Yeah thats unsafe but I don't think somebody uses this func
    lock::CSLock mylock(fcs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return ReadFileExO(hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped,
                           lpCompletionRoutine);
    auto h = *it;
    if (!h->reading) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    mylock.unlock();
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
    if (state::is_save_handle(hFile))
        return WriteFileExO(hFile, lpBuffer, nNumberOfBytesToWrite, lpOverlapped,
                            lpCompletionRoutine);
    lock::CSLock mylock(fcs);
    auto it = std::find(our_handles.begin(), our_handles.end(), hFile);
    if (it == our_handles.end())
        return WriteFileExO(hFile, lpBuffer, nNumberOfBytesToWrite, lpOverlapped,
                            lpCompletionRoutine);
    auto h = *it;
    if (!h->writing) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    mylock.unlock();
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
    if (state::is_save_handle(hFile))
        return SetFilePointerO(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    lock::CSLock mylock(fcs);
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
    if (state::is_save_handle(hFile))
        return GetFileSizeO(hFile, lpFileSizeHigh);
    lock::CSLock mylock(fcs);
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
    if (state::is_save_handle(hFile))
        return SetFilePointerExO(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    lock::CSLock mylock(fcs);
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
    if (state::is_save_handle(hFile))
        return GetFileSizeExO(hFile, lpFileSize);
    lock::CSLock mylock(fcs);
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

static DWORD of_style_to_disposition(UINT uStyle) {
    if (uStyle & OF_CREATE)
        return CREATE_ALWAYS;
    if (uStyle & OF_EXIST)
        return OPEN_EXISTING;
    return OPEN_ALWAYS;
}

static HFILE(WINAPI* OpenFileO)(LPCSTR, LPOFSTRUCT, UINT);
static HFILE WINAPI OpenFileH(LPCSTR lpFileName, LPOFSTRUCT lpReOpenBuff, UINT uStyle) {
    if (!lpFileName || !lpReOpenBuff)
        return HFILE_ERROR;
    auto fp = uconv::from_ansi(lpFileName);
    lpReOpenBuff->cBytes = sizeof(OFSTRUCT);
    lpReOpenBuff->fFixedDisk = 1;
    lpReOpenBuff->nErrCode = 0;
    strcpy(lpReOpenBuff->szPathName, lpFileName);
    if (uStyle & OF_DELETE) {
        string norm = normalize_path(uconv::from_ansi(lpFileName));
        lock::CSLock mylock(fcs);
        auto it = file_map.find(norm);
        if (it != file_map.end() && it->second.data) {
            std::free(it->second.data);
            it->second.data = nullptr;
            it->second.size = 0;
            return TRUE;
        }
        return HFILE_ERROR;
    }
    bool for_read = (uStyle & OF_WRITE) == 0;
    bool for_write = (uStyle & (OF_WRITE | OF_READWRITE)) != 0;
    DWORD disposition = of_style_to_disposition(uStyle);
    auto temp_ret =
        handle_file_open(uconv::from_ansi(lpFileName), for_read, for_write, disposition);
    if (temp_ret.has_value()) {
        void* h = temp_ret.value();
        if (h == INVALID_HANDLE_VALUE) {
            lpReOpenBuff->nErrCode = (WORD)GetLastError();
            return HFILE_ERROR;
        }
        return reinterpret_cast<HFILE>(h);
    }
    return OpenFileO(lpFileName, lpReOpenBuff, uStyle);
}

static BOOL(WINAPI* DeleteFileAO)(LPCSTR lpFileName);
static BOOL WINAPI DeleteFileAH(LPCSTR lpFileName) {
    string norm = normalize_path(uconv::from_ansi(lpFileName));
    // spdlog::debug("DeleteFileA: {}", norm);
    lock::CSLock mylock(fcs);
    auto it = file_map.find(norm);
    if (it != file_map.end() && it->second.data) {
        if (it->second.refcount != 0)
            return FALSE; // WTF
        std::free(it->second.data);
        it->second.data = nullptr;
        it->second.size = 0;
        return TRUE;
    }
    // Passing through because otherwise it will break temp dir cleanup
    return DeleteFileAO(lpFileName);
}

static BOOL(WINAPI* DeleteFileWO)(LPCWSTR lpFileName);
static BOOL WINAPI DeleteFileWH(LPCWSTR lpFileName) {
    string norm = normalize_path(uconv::from_utf16(lpFileName));
    // spdlog::debug("DeleteFileW: {}", norm);
    lock::CSLock mylock(fcs);
    auto it = file_map.find(norm);
    if (it != file_map.end() && it->second.data) {
        if (it->second.refcount != 0)
            return FALSE;
        std::free(it->second.data);
        it->second.data = nullptr;
        it->second.size = 0;
        return TRUE;
    }
    return DeleteFileWO(lpFileName);
}

static void parse_crt_flags(int oflag, bool& r, bool& w) {
    r = (oflag & _O_WRONLY) == 0;
    w = (oflag & (_O_WRONLY | _O_RDWR)) != 0;
}

static DWORD crt_flags_to_disposition(int oflag) {
    if (oflag & _O_CREAT) {
        if (oflag & _O_EXCL)
            return CREATE_NEW;
        if (oflag & _O_TRUNC)
            return CREATE_ALWAYS;
        return OPEN_ALWAYS;
    }
    if (oflag & _O_TRUNC)
        return TRUNCATE_EXISTING;
    return OPEN_EXISTING;
}

static int (*_openO)(const char*, int, int);
static int _openH(const char* filename, int oflag, int pmode) {
    auto fp = uconv::from_ansi(filename);
    bool r, w;
    parse_crt_flags(oflag, r, w);
    auto temp_ret = handle_file_open(fp, r, w, crt_flags_to_disposition(oflag));
    if (temp_ret.has_value())
        return static_cast<int>(reinterpret_cast<uintptr_t>(temp_ret.value()));
    return _openO(filename, oflag, pmode);
}

static int (*_wopenO)(const wchar_t*, int, int);
static int _wopenH(const wchar_t* filename, int oflag, int pmode) {
    auto fp = uconv::from_utf16(filename);
    bool r, w;
    parse_crt_flags(oflag, r, w);
    auto temp_ret = handle_file_open(fp, r, w, crt_flags_to_disposition(oflag));
    if (temp_ret.has_value())
        return static_cast<int>(reinterpret_cast<uintptr_t>(temp_ret.value()));
    return _wopenO(filename, oflag, pmode);
}

static bool is_our_fd(int fd) {
    // I think nobody uses it so we can be slow
    lock::CSLock mylock(fcs);
    auto it = std::find(our_handles.begin(), our_handles.end(), reinterpret_cast<FileHandle*>(fd));
    return it != our_handles.end();
}

static int(CDECL* _readO)(int, void*, unsigned int);
static int CDECL _readH(int fd, void* buffer, unsigned int count) {
    if (is_our_fd(fd)) {
        DWORD bytesRead = 0;
        if (ReadFileH(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(fd)), buffer, count,
                      &bytesRead, nullptr))
            return static_cast<int>(bytesRead);
        return -1;
    }
    return _readO(fd, buffer, count);
}

static int(CDECL* _writeO)(int, const void*, unsigned int);
static int CDECL _writeH(int fd, const void* buffer, unsigned int count) {
    if (is_our_fd(fd)) {
        DWORD bytesWritten = 0;
        if (WriteFileH(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(fd)), buffer, count,
                       &bytesWritten, nullptr))
            return static_cast<int>(bytesWritten);
        return -1;
    }
    return _writeO(fd, buffer, count);
}

static long(CDECL* _lseekO)(int, long, int);
static long CDECL _lseekH(int fd, long offset, int origin) {
    if (is_our_fd(fd)) {
        // origin CRT is the same with FILE_BEGIN/CURRENT/END in WinAPI (0, 1, 2)
        DWORD res = SetFilePointerH(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(fd)), offset,
                                    nullptr, (DWORD)origin);
        return (res == INVALID_SET_FILE_POINTER) ? -1L : (long)res;
    }
    return _lseekO(fd, offset, origin);
}

static BOOL WINAPI CloseHandleH(HANDLE hObject) {
    lock::CSLock mylock(fcs);
    auto it =
        std::find(our_handles.begin(), our_handles.end(), reinterpret_cast<FileHandle*>(hObject));
    if (it == our_handles.end())
        return CloseHandleO(hObject);
    delete *it;
    our_handles.erase(it);
    return TRUE;
}

static int(CDECL* _closeO)(int);
static int CDECL _closeH(int fd) {
    if (is_our_fd(fd)) {
        // Slow
        if (CloseHandleH(reinterpret_cast<HANDLE>(static_cast<uintptr_t>(fd))))
            return 0;
        return -1;
    }
    return _closeO(fd);
}

static HFILE(WINAPI* _lopenO)(LPCSTR lpPathName, int iReadWrite);
static HFILE WINAPI _lopenH(LPCSTR lpPathName, int iReadWrite) {
    spdlog::error("TODO: _lopen");
    return HFILE_ERROR;
}

static HFILE(WINAPI* _lcloseO)(HFILE);
static HFILE WINAPI _lcloseH(HFILE hFile) {
    lock::CSLock mylock(fcs);
    auto it =
        std::find(our_handles.begin(), our_handles.end(), reinterpret_cast<FileHandle*>(hFile));
    if (it == our_handles.end())
        return _lcloseO(hFile);
    delete *it;
    our_handles.erase(it);
    return 0;
}

static int (*_accessO)(const char* path, int mode);
static int _accessH(const char* path, int mode) {
    string norm_fp = normalize_path(uconv::from_ansi(path));
    lock::CSLock mylock(fcs);
    auto it = file_map.find(norm_fp);
    if (it == file_map.end()) {
        mylock.unlock();
        return _accessO(path, mode);
    }
    if ((mode == 2 || mode == 6) && !it->second.allow_write)
        return -1;
    if ((mode == 4 || mode == 6) && !it->second.allow_read)
        return -1;
    return 0;
}

static int (*_waccessO)(const wchar_t* path, int mode);
static int _waccessH(const wchar_t* path, int mode) {
    string norm_fp = normalize_path(uconv::from_utf16(path));
    lock::CSLock mylock(fcs);
    auto it = file_map.find(norm_fp);
    if (it == file_map.end()) {
        mylock.unlock();
        return _waccessO(path, mode);
    }
    if ((mode == 2 || mode == 6) && !it->second.allow_write)
        return -1;
    if ((mode == 4 || mode == 6) && !it->second.allow_read)
        return -1;
    return 0;
}

static void parse_stdio_mode(const char* mode, bool& r, bool& w, bool& plus) {
    r = strchr(mode, 'r') != nullptr;
    w = (strchr(mode, 'w') != nullptr) || (strchr(mode, 'a') != nullptr);
    plus = strchr(mode, '+') != nullptr;
}

static DWORD stdio_mode_to_disposition(const char* mode) {
    if (strchr(mode, 'w'))
        return CREATE_ALWAYS;
    if (strchr(mode, 'a'))
        return OPEN_ALWAYS;
    if (strchr(mode, 'r'))
        return OPEN_EXISTING;
    return OPEN_EXISTING;
}

static FILE* (*fopenO)(const char*, const char*);
static FILE* fopenH(const char* filename, const char* mode) {
    bool r, w, plus;
    parse_stdio_mode(mode, r, w, plus);
    // spdlog::debug("fopen {} {}", uconv::from_ansi(filename), mode);
    auto temp_ret = handle_file_open(
        uconv::from_ansi(filename), r || plus, w || plus,
        crt_flags_to_disposition(strchr(mode, 'w') ? _O_CREAT | _O_TRUNC : _O_RDONLY));
    if (temp_ret.has_value()) {
        if (temp_ret.value() == INVALID_HANDLE_VALUE)
            return nullptr;
        return reinterpret_cast<FILE*>(temp_ret.value());
    }
    return fopenO(filename, mode);
}

static FILE* (*_wfopenO)(const wchar_t*, const wchar_t*);
static FILE* _wfopenH(const wchar_t* filename, const wchar_t* mode) {
    bool r, w, plus;
    auto modeA = uconv::from_utf16(mode);
    parse_stdio_mode(modeA.c_str(), r, w, plus);
    auto temp_ret = handle_file_open(
        uconv::from_utf16(filename), r || plus, w || plus,
        crt_flags_to_disposition(strchr(modeA.c_str(), 'w') ? _O_CREAT | _O_TRUNC : _O_RDONLY));
    if (temp_ret.has_value()) {
        if (temp_ret.value() == INVALID_HANDLE_VALUE)
            return nullptr;
        return reinterpret_cast<FILE*>(temp_ret.value());
    }
    return _wfopenO(filename, mode);
}

static size_t (*freadO)(void*, size_t, size_t, FILE*);
static size_t freadH(void* buffer, size_t size, size_t count, FILE* stream) {
    if (is_our_fd(reinterpret_cast<int>(stream))) {
        DWORD read = 0;
        if (size > 0 &&
            ReadFileH(reinterpret_cast<HANDLE>(stream), buffer, size * count, &read, nullptr))
            return static_cast<size_t>(read / size);
        return 0;
    }
    return freadO(buffer, size, count, stream);
}

static size_t (*fwriteO)(const void*, size_t, size_t, FILE*);
static size_t fwriteH(const void* buffer, size_t size, size_t count, FILE* stream) {
    if (is_our_fd(reinterpret_cast<int>(stream))) {
        DWORD written = 0;
        if (WriteFileH(reinterpret_cast<HANDLE>(stream), buffer, size * count, &written, nullptr))
            return static_cast<size_t>(written / size);
        return 0;
    }
    return fwriteO(buffer, size, count, stream);
}

static int (*fseekO)(FILE*, long, int);
static int fseekH(FILE* stream, long offset, int origin) {
    if (is_our_fd(reinterpret_cast<int>(stream))) {
        return (SetFilePointerH(reinterpret_cast<HANDLE>(stream), offset, nullptr, (DWORD)origin) ==
                INVALID_SET_FILE_POINTER)
                   ? -1
                   : 0;
    }
    return fseekO(stream, offset, origin);
}

static long (*ftellO)(FILE*);
static long ftellH(FILE* stream) {
    if (is_our_fd(reinterpret_cast<int>(stream))) {
        lock::CSLock mylock(fcs);
        return static_cast<long>(reinterpret_cast<FileHandle*>(stream)->pos);
    }
    return ftellO(stream);
}

static int (*fcloseO)(FILE*);
static int fcloseH(FILE* stream) {
    if (is_our_fd(reinterpret_cast<int>(stream))) {
        return CloseHandleH(reinterpret_cast<HANDLE>(stream)) ? 0 : -1;
    }
    return fcloseO(stream);
}

static int (*fflushO)(FILE*);
static int fflushH(FILE* stream) {
    if (is_our_fd(reinterpret_cast<int>(stream)))
        return 0;
    return fflushO(stream);
}

static int (*fgetcO)(FILE*);
static int fgetcH(FILE* stream) {
    if (is_our_fd(reinterpret_cast<int>(stream))) {
        unsigned char c;
        DWORD read = 0;
        if (ReadFileH(reinterpret_cast<HANDLE>(stream), &c, 1, &read, nullptr) && read == 1)
            return c;
        return EOF;
    }
    return fgetcO(stream);
}

static bool ProcessVirtualIni(string_view fileName, std::function<void(CSimpleIniA&)> action,
                              bool save = false) {
    // FIXME: why so slow
    CSimpleIniA ini;
    ini.SetUnicode();

    ofs::File file;
    if (file.open(fileName, 0, true)) {
        size_t size = static_cast<size_t>(file.size());
        std::string content(size, '\0');
        file.read(&content[0], size);
        file.close();
        if (ini.LoadData(content) < 0)
            return false;
    }

    action(ini);

    if (save) {
        std::string out;
        if (ini.Save(out) >= 0) {
            if (file.open(fileName, 1, true)) {
                file.write(out.data(), out.size());
                file.close();
                return true;
            }
        }
        return false;
    }
    return true;
}

static DWORD WINAPI GetPrivateProfileStringAH(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault,
                                              LPSTR lpReturnedString, DWORD nSize,
                                              LPCSTR lpFileName) {
    if (!lpFileName || nSize == 0)
        return 0;

    std::string fn = uconv::from_ansi(lpFileName);
    std::string result = lpDefault ? lpDefault : "";

    ProcessVirtualIni(fn, [&](CSimpleIniA& ini) {
        result = ini.GetValue(lpAppName ? lpAppName : "", lpKeyName ? lpKeyName : "",
                              lpDefault ? lpDefault : "");
    });

    size_t len = result.copy(lpReturnedString, nSize - 1);
    lpReturnedString[len] = '\0';

    spdlog::debug("GetA: File={}, Sec=[{}], Key={}, Res={}", fn, lpAppName ? lpAppName : "",
                  lpKeyName ? lpKeyName : "", lpReturnedString);
    return static_cast<DWORD>(len);
}

static BOOL WINAPI WritePrivateProfileStringAH(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString,
                                               LPCSTR lpFileName) {
    if (!lpFileName || !lpAppName || !lpKeyName)
        return FALSE;

    std::string fn = uconv::from_ansi(lpFileName);
    bool ok = ProcessVirtualIni(
        fn, [&](CSimpleIniA& ini) { ini.SetValue(lpAppName, lpKeyName, lpString); }, true);

    spdlog::debug("WriteA: File={}, Sec=[{}], Key={}, Ok={}", fn, lpAppName, lpKeyName, ok);
    return ok ? TRUE : FALSE;
}

static DWORD WINAPI GetPrivateProfileStringWH(LPCWSTR lpAppName, LPCWSTR lpKeyName,
                                              LPCWSTR lpDefault, LPWSTR lpReturnedString,
                                              DWORD nSize, LPCWSTR lpFileName) {
    if (!lpFileName || nSize == 0)
        return 0;

    std::string fn = uconv::from_utf16(lpFileName);
    std::string app = lpAppName ? uconv::from_utf16(lpAppName) : "";
    std::string key = lpKeyName ? uconv::from_utf16(lpKeyName) : "";
    std::string def = lpDefault ? uconv::from_utf16(lpDefault) : "";

    std::string result = def;
    ProcessVirtualIni(fn, [&](CSimpleIniA& ini) {
        result = ini.GetValue(app.c_str(), key.c_str(), def.c_str());
    });

    wchar_t* resW = uconv::to_utf16(result.c_str());
    size_t len = 0;
    if (resW) {
        len = std::wcslen(resW);
        if (len >= nSize)
            len = nSize - 1;
        std::wcsncpy(lpReturnedString, resW, len);
        std::free(resW);
    }
    lpReturnedString[len] = L'\0';

    spdlog::debug("GetW: File={}, Sec=[{}], Key={}", fn, app, key);
    return static_cast<DWORD>(len);
}

static BOOL WINAPI WritePrivateProfileStringWH(LPCWSTR lpAppName, LPCWSTR lpKeyName,
                                               LPCWSTR lpString, LPCWSTR lpFileName) {
    if (!lpFileName || !lpAppName || !lpKeyName)
        return FALSE;

    std::string fn = uconv::from_utf16(lpFileName);
    std::string app = uconv::from_utf16(lpAppName);
    std::string key = uconv::from_utf16(lpKeyName);
    std::string val = lpString ? uconv::from_utf16(lpString) : "";

    bool ok = ProcessVirtualIni(
        fn,
        [&](CSimpleIniA& ini) {
            ini.SetValue(app.c_str(), key.c_str(), lpString ? val.c_str() : nullptr);
        },
        true);

    spdlog::debug("WriteW: File={}, Sec=[{}], Key={}, Ok={}", fn, app, key, ok);
    return ok ? TRUE : FALSE;
}

void files::init() {
    // IAT_AUTO("kernel32.dll", SetCurrentDirectoryW);
    IAT_STR_ONLY("kernel32.dll", GetTempPath);
}

void files::hook_fs() {
    IAT_STR_AUTO("kernel32.dll", DeleteFile);
    IAT_STR_AUTO("kernel32.dll", CreateFile);
    if (!conf::get().no_ini_hooks) {
        // Why this shit is so slow
        IAT_STR_ONLY("kernel32.dll", WritePrivateProfileString);
        IAT_STR_ONLY("kernel32.dll", GetPrivateProfileString);
    }
    IAT_AUTO("kernel32.dll", OpenFile);
    IAT_AUTO("kernel32.dll", ReadFile);
    IAT_AUTO("kernel32.dll", ReadFileEx);
    IAT_AUTO("kernel32.dll", WriteFile);
    IAT_AUTO("kernel32.dll", WriteFileEx);
    IAT_AUTO("kernel32.dll", SetFilePointer);
    IAT_AUTO("kernel32.dll", SetFilePointerEx);
    IAT_AUTO("kernel32.dll", GetFileSize);
    IAT_AUTO("kernel32.dll", GetFileSizeEx);
    IAT_AUTO("kernel32.dll", CloseHandle);
    IAT_AUTO("kernel32.dll", _lopen);
    IAT_AUTO("kernel32.dll", _lclose);
    IAT_AUTO("msvcrt.dll", _open);
    IAT_AUTO("msvcrt.dll", _wopen);
    IAT_AUTO("msvcrt.dll", _read);
    IAT_AUTO("msvcrt.dll", _write);
    IAT_AUTO("msvcrt.dll", _lseek);
    IAT_AUTO("msvcrt.dll", _close);
    IAT_AUTO("msvcrt.dll", _access);
    IAT_AUTO("msvcrt.dll", _waccess);
    IAT_AUTO("msvcrt.dll", fopen);
    IAT_AUTO("msvcrt.dll", _wfopen);
    IAT_AUTO("msvcrt.dll", fread);
    IAT_AUTO("msvcrt.dll", fwrite);
    IAT_AUTO("msvcrt.dll", fseek);
    IAT_AUTO("msvcrt.dll", ftell);
    IAT_AUTO("msvcrt.dll", fclose);
    IAT_AUTO("msvcrt.dll", fflush);
    IAT_AUTO("msvcrt.dll", fgetc);
    spdlog::info("Virtual FS hooks installed");
}

void files::draw_ui() {
    ImGui::Text("Opened handles: %i", static_cast<int>(our_handles.size()));
    for (auto& it : file_map) {
        if (it.second.data != nullptr)
            ImGui::Text("%s: %i bytes", it.first.c_str(), static_cast<int>(it.second.size));
        else
            ImGui::Text("%s: deleted", it.first.c_str());
    }
}

void files::clear_fs() {
    lock::CSLock mylock(fcs);
    auto it = file_map.begin();
    while (it != file_map.end()) {
        if (it->second.refcount > 0) {
            spdlog::error("Cannot clear file '{}' because it is in use (refcount: {})", it->first,
                          it->second.refcount);
            ++it;
        } else {
            if (it->second.data) {
                std::free(it->second.data);
                it->second.data = nullptr;
                it->second.size = 0;
            }
            it = file_map.erase(it);
        }
    }
}

bool files::save_fs(ofs::File& file) {
    if (!conf::get().save_vfs) {
        state::write_bin(file, static_cast<size_t>(0));
        return true;
    }
    ENSURE(our_handles.empty());
    lock::CSLock mylock(fcs);
    state::write_bin(file, file_map.size());
    for (auto& [fn, data] : file_map) {
        state::write_bin(file, fn);
        state::write_bin(file, data.size);
        state::write_bin(file, data.allow_read);
        state::write_bin(file, data.allow_write);
        auto w_ret = file.write(data.data, data.size);
        ENSURE(w_ret);
    }
    return true;
}

bool files::load_fs(ofs::File& file) {
    clear_fs();
    lock::CSLock mylock(fcs);
    size_t total;
    state::load_bin(file, total);
    if (total == 0)
        return true;
    for (size_t i = 0; i < total; i++) {
        std::string fn;
        state::load_bin(file, fn);
        if (file_map.find(fn) != file_map.end()) {
            // WTF
            ENSURE(false);
            continue;
        }
        FileData data;
        state::load_bin(file, data.size);
        state::load_bin(file, data.allow_read);
        state::load_bin(file, data.allow_write);
        data.data = std::malloc(data.size);
        ENSURE(data.data != nullptr);
        auto w_ret = file.read(data.data, data.size);
        ENSURE(w_ret);
        file_map[std::move(fn)] = std::move(data);
    }
    return true;
}
