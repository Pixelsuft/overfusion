#define WIN32_LEAN_AND_MEAN
#include "subprocess.hpp"
#include "ass.hpp"
#include "uconv.hpp"
#include <spdlog/spdlog.h>

using subprocess::Process;

Process::Process() {
    pi.hProcess = pi.hThread = nullptr;
    pi.dwProcessId = pi.dwThreadId = 0;
    hChildStdinRead = nullptr;
    hChildStdinWrite = nullptr;
}

Process::~Process() {
    if (is_open())
        close();
}

bool Process::is_open() { return pi.hProcess != nullptr; }

bool Process::open(ost::string_view cmdline) {
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
    auto ret = true;
    if (!CreateProcessW(nullptr, w_buf, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        spdlog::error("Failed to create child process");
        CloseHandle(hChildStdinWrite);
        hChildStdinWrite = nullptr;
        ret = false;
    }
    std::free(w_buf);
    CloseHandle(hChildStdinRead);
    hChildStdinRead = nullptr;
    return true;
}

bool Process::close() {
    ASS(!is_open());
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
