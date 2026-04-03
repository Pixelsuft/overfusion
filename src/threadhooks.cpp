#define WIN32_LEAN_AND_MEAN
#include "threadhooks.hpp"
#include "mem.hpp"
#include <Windows.h>
#include <processthreadsapi.h>
#include <spdlog/spdlog.h>

static HANDLE(WINAPI* CreateThreadO)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID,
                                     DWORD, LPDWORD);
static HANDLE WINAPI CreateThreadH(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                                   LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
                                   DWORD dwCreationFlags, LPDWORD lpThreadId) {
    // return nullptr;
    auto ret = CreateThreadO(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter,
                             dwCreationFlags, lpThreadId);
    spdlog::debug("CreateThread: {}", reinterpret_cast<void*>(lpStartAddress));
    return ret;
}

void threadhooks::update_init() {}

void threadhooks::pre_init() { HOOK_AUTO("kernel32.dll", CreateThread); }
