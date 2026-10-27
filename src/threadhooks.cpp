#define WIN32_LEAN_AND_MEAN
#include "threadhooks.hpp"
#include "config.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "uconv.hpp"
#include <Windows.h>
#include <tlhelp32.h>

static HANDLE(WINAPI* CreateThreadO)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID,
                                     DWORD, LPDWORD);
static HANDLE WINAPI CreateThreadH(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                                   LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
                                   DWORD dwCreationFlags, LPDWORD lpThreadId) {
    if (conf::get().disable_threads)
        return nullptr;
    if (false)
        of::debug("CreateThread: {}", reinterpret_cast<void*>(lpStartAddress));
    auto ret = CreateThreadO(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter,
                             dwCreationFlags, lpThreadId);
    return ret;
}

static BOOL WINAPI CreateProcessAH(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                                   LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                   LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                                   DWORD dwCreationFlags, LPVOID lpEnvironment,
                                   LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo,
                                   LPPROCESS_INFORMATION lpProcessInformation) {
    auto app_name = lpApplicationName ? uconv::from_ansi(lpApplicationName) : "nullptr";
    auto cmd_line = lpCommandLine ? uconv::from_ansi(lpCommandLine) : "nullptr";
    of::warn("CreateProcessA {}: {}", app_name, cmd_line);
    return FALSE;
}

BOOL(WINAPI* CreateProcessWO)(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                              LPSECURITY_ATTRIBUTES lpProcessAttributes,
                              LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                              DWORD dwCreationFlags, LPVOID lpEnvironment,
                              LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
                              LPPROCESS_INFORMATION lpProcessInformation);
static BOOL WINAPI CreateProcessWH(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                                   LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                   LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                                   DWORD dwCreationFlags, LPVOID lpEnvironment,
                                   LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
                                   LPPROCESS_INFORMATION lpProcessInformation) {
    auto app_name = lpApplicationName ? uconv::from_utf16(lpApplicationName) : "nullptr";
    auto cmd_line = lpCommandLine ? uconv::from_utf16(lpCommandLine) : "nullptr";
    of::warn("CreateProcessW {}: {}", app_name, cmd_line);
    return FALSE;
}

static VOID(WINAPI* ExitProcessO)(UINT uExitCode);
static VOID WINAPI ExitProcessH(UINT uExitCode) {
    of::info("ExitProcess {}", uExitCode);
    ExitProcessO(uExitCode);
}

static HANDLE WINAPI OpenProcessH(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
    of::warn("Failing OpenProcess");
    return nullptr;
}

static HANDLE(WINAPI* CreateToolhelp32SnapshotO)(DWORD dwFlags, DWORD th32ProcessID);
static HANDLE WINAPI CreateToolhelp32SnapshotH(DWORD dwFlags, DWORD th32ProcessID) {
    // Bypass our IAT stuff
    if (dwFlags == (TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32) && th32ProcessID == 0)
        return CreateToolhelp32SnapshotO(dwFlags, th32ProcessID);
    // Bypass MinHook stuff
    if (dwFlags == TH32CS_SNAPTHREAD && th32ProcessID == 0)
        return CreateToolhelp32SnapshotO(dwFlags, th32ProcessID);
    // process.mfx hack
    of::warn("Failing CreateToolhelp32Snapshot");
    return INVALID_HANDLE_VALUE;
}

static BOOL WINAPI OpenProcessTokenH(HANDLE ProcessHandle, DWORD DesiredAccess,
                                     PHANDLE TokenHandle) {
    of::warn("Failing OpenProcessToken");
    return FALSE;
}

void threadhooks::update_init() {
    if (conf::get().delay_thread_hook)
        HOOK_AUTO("kernel32.dll", CreateThread);
}

void threadhooks::pre_init() {
    // TODO: deal with process.mfx plugin
    if (!conf::get().delay_thread_hook)
        HOOK_AUTO("kernel32.dll", CreateThread);
    HOOK_ONLY("kernel32.dll", OpenProcess);
    HOOK_ONLY("kernel32.dll", CreateProcessA);
    HOOK_AUTO("kernel32.dll", CreateProcessW);
    HOOK_AUTO("kernel32.dll", ExitProcess);
    HOOK_AUTO("kernel32.dll", CreateToolhelp32Snapshot);
    IAT_ONLY("advapi32.dll", OpenProcessToken);
}
