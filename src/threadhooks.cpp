#define WIN32_LEAN_AND_MEAN
#include "threadhooks.hpp"
#include "config.hpp"
#include "mem.hpp"
#include "uconv.hpp"
#include <Windows.h>
#include <processthreadsapi.h>
#include <spdlog/spdlog.h>

static HANDLE(WINAPI* CreateThreadO)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID,
                                     DWORD, LPDWORD);
static HANDLE WINAPI CreateThreadH(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                                   LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
                                   DWORD dwCreationFlags, LPDWORD lpThreadId) {
    if (false)
        spdlog::debug("CreateThread: {}", reinterpret_cast<void*>(lpStartAddress));
    if (conf::get().disable_threads)
        return nullptr;
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
    spdlog::warn("CreateProcessA {}: {}", app_name, cmd_line);
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
    spdlog::warn("CreateProcessW {}: {}", app_name, cmd_line);
    return FALSE;
}

void threadhooks::update_init() {
    if (conf::get().delay_thread_hook)
        HOOK_AUTO("kernel32.dll", CreateThread);
}

void threadhooks::pre_init() {
    if (!conf::get().delay_thread_hook)
        HOOK_AUTO("kernel32.dll", CreateThread);
    HOOK_ONLY("kernel32.dll", CreateProcessA);
    HOOK_AUTO("kernel32.dll", CreateProcessW);
}
