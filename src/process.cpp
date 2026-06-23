#define WIN32_LEAN_AND_MEAN
#include "process.hpp"
#include "ass.hpp"
#include "uconv.hpp"
#include <spdlog/spdlog.h>

using process::Subprocess;

extern BOOL(WINAPI* CreateProcessWO)(LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES,
                                     BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW,
                                     LPPROCESS_INFORMATION);

Subprocess::Subprocess() {
    pi.hProcess = pi.hThread = nullptr;
    pi.dwProcessId = pi.dwThreadId = 0;
    hChildStdinRead = nullptr;
    hChildStdinWrite = nullptr;
}

Subprocess::~Subprocess() {
    if (is_open())
        close();
}

bool Subprocess::is_open() { return pi.hProcess != nullptr; }

bool Subprocess::open(ost::string_view cmdline) {
    ASS(!is_open());
    SECURITY_ATTRIBUTES saAttr;
    ZeroMemory(&saAttr, sizeof(saAttr));
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;
    auto pipe_ret = CreatePipe(&hChildStdinRead, &hChildStdinWrite, &saAttr, 0);
    if (!pipe_ret) {
        spdlog::error("Failed to create pipes for the process");
        return false;
    }
    if (!SetHandleInformation(hChildStdinWrite, HANDLE_FLAG_INHERIT, 0)) {
        spdlog::error("Failed to set stdout handle information");
        CloseHandle(hChildStdinWrite);
        CloseHandle(hChildStdinRead);
        hChildStdinRead = hChildStdinWrite = nullptr;
        return false;
    }
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = hChildStdinRead;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    wchar_t* w_buf = uconv::to_utf16(cmdline);
    ENSURE(w_buf != nullptr);
    auto ret = true;
    if (!CreateProcessWO(nullptr, w_buf, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        spdlog::error("Failed to create child process");
        CloseHandle(hChildStdinWrite);
        pi.hProcess = pi.hThread = nullptr;
        pi.dwProcessId = pi.dwThreadId = 0;
        hChildStdinWrite = nullptr;
        ret = false;
    }
    std::free(w_buf);
    CloseHandle(hChildStdinRead);
    hChildStdinRead = nullptr;
    return ret;
}

bool Subprocess::write(const void* data, size_t size) {
    ENSURE(is_open());
    DWORD dwWritten;
    BOOL ret = WriteFile(hChildStdinWrite, data, size, &dwWritten, nullptr);
    return ret && static_cast<size_t>(dwWritten) == size;
}

bool Subprocess::close() {
    ENSURE(is_open());
    if (hChildStdinWrite) {
        CloseHandle(hChildStdinWrite);
        hChildStdinWrite = nullptr;
    }
    if (hChildStdinRead) {
        CloseHandle(hChildStdinRead);
        hChildStdinRead = nullptr;
    }
    if (pi.hProcess) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        pi.hProcess = nullptr;
    }
    if (pi.hThread) {
        CloseHandle(pi.hThread);
        pi.hThread = nullptr;
    }
    pi.dwProcessId = pi.dwThreadId = 0;
    return true;
}

ost::optional<std::string> process::get_env(ost::string_view key) {
    auto key_w = uconv::to_utf16(key);
    ENSURE(key_w);
    wchar_t buf[512];
    auto len = GetEnvironmentVariableW(key_w, buf, 512);
    std::free(key_w);
    if (len > 510 || len == 0) {
        return {};
    }
    return uconv::from_utf16(buf);
}
