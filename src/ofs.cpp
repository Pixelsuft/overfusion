#define WIN32_LEAN_AND_MEAN
#include "ofs.hpp"
#include "ass.hpp"
#include "sv.hpp"
#include "uconv.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ofs::File;
using ost::string_view;
using std::string;

extern HANDLE(WINAPI* CreateFileWO)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                    HANDLE);
extern HANDLE WINAPI CreateFileWH(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                  HANDLE);
extern BOOL(WINAPI* CloseHandleO)(HANDLE);

static string real_cwd;

File::File() noexcept { handle = INVALID_HANDLE_VALUE; }

File::File(string_view path, int mode) noexcept {
    handle = INVALID_HANDLE_VALUE;
    open(path, mode);
}

File::File(File&& other) noexcept : handle(other.handle) { other.handle = INVALID_HANDLE_VALUE; }

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        close();
        handle = other.handle;
        other.handle = INVALID_HANDLE_VALUE;
    }
    return *this;
}

bool File::open(string_view path, int mode, bool hooked) {
    if (is_open())
        close();
    wchar_t* w_path = uconv::to_utf16(path);
    ENSURE(w_path != nullptr);
    handle = static_cast<void*>((hooked ? CreateFileWH : CreateFileWO)(
        w_path, mode == 1 ? (GENERIC_WRITE | DELETE) : GENERIC_READ,
        mode == 1 ? 0 : FILE_SHARE_READ, nullptr, mode == 1 ? CREATE_ALWAYS : OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr));
    std::free(w_path);
    return is_open();
}

bool File::is_open() { return handle != INVALID_HANDLE_VALUE; }

bool File::readln(std::string& line) {
    ASS(is_open());
    line.clear();
    line.reserve(128);
    char buffer;
    DWORD bytesRead;
    while (ReadFile(handle, &buffer, 1, &bytesRead, nullptr) && bytesRead > 0) {
        if (buffer == '\n') {
            if (line.size() == 0)
                continue;
            break;
        }
        if (buffer != '\r')
            line += buffer;
    }
    return line.size() > 0;
}

bool File::read(void* buf, size_t size) {
    ASS(is_open());
    DWORD bytesRead;
    return ReadFile(handle, buf, (DWORD)size, &bytesRead, nullptr) && (DWORD)size == bytesRead;
}

bool File::write(const void* buf, size_t size) {
    ASS(is_open());
    DWORD bytesWritten;
    return WriteFile(handle, buf, (DWORD)size, &bytesWritten, nullptr) &&
           (DWORD)size == bytesWritten;
}

bool File::seek(long long offset, ofs::SeekMode mode) {
    ASS(is_open());

    LARGE_INTEGER liOffset;
    liOffset.QuadPart = offset;

    DWORD moveMethod;
    switch (mode) {
    case SeekCurrent:
        moveMethod = FILE_CURRENT;
        break;
    case SeekEnd:
        moveMethod = FILE_END;
        break;
    default:
        moveMethod = FILE_BEGIN;
        break;
    }

    return SetFilePointerEx(handle, liOffset, nullptr, moveMethod) != 0;
}

long long File::tell() {
    ASS(is_open());
    LARGE_INTEGER liOffset;
    liOffset.QuadPart = 0;
    LARGE_INTEGER newPos;
    if (SetFilePointerEx(handle, liOffset, &newPos, FILE_CURRENT))
        return newPos.QuadPart;
    return -1;
}

long long File::size() {
    ASS(is_open());
    LARGE_INTEGER ret;
    if (GetFileSizeEx(handle, &ret))
        return ret.QuadPart;
    return -1;
}

void File::close() {
    if (is_open()) {
        ENSURE(CloseHandle(handle));
        handle = INVALID_HANDLE_VALUE;
    }
}

bool ofs::remove_file(string_view path) {
    wchar_t* w_path = uconv::to_utf16(path);
    ENSURE(w_path != nullptr);
    bool ret = (DeleteFileW(w_path) != FALSE);
    if (!ret && GetLastError() != ERROR_FILE_NOT_FOUND)
        spdlog::error("Failed to remove file: {}", path);
    std::free(w_path);
    return ret;
}

bool ofs::make_dir(ost::string_view path) {
    wchar_t* converted = uconv::to_utf16(path);
    ENSURE(converted != nullptr);
    auto ret = CreateDirectoryW(converted, nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
    std::free(converted);
    return ret;
}

bool ofs::file_exists(ost::string_view path) {
    wchar_t* converted = uconv::to_utf16(path);
    ENSURE(converted != nullptr);
    auto dwAttrib = GetFileAttributesW(converted);
    std::free(converted);
    return dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}

void ofs::pre_init() {
    wchar_t buffer[MAX_PATH];
    auto len_ret = GetCurrentDirectoryW(MAX_PATH, buffer);
    ENSURE(len_ret > 2 && len_ret < MAX_PATH);
    buffer[len_ret] = L'\0';
    real_cwd = uconv::from_utf16(buffer);
}

string_view ofs::get_cwd() { return real_cwd; }
