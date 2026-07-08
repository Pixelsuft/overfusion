#define WIN32_LEAN_AND_MEAN
#include "mmfshooks.hpp"
#include "ass.hpp"
#include "log.hpp"
#include "mem.hpp"
#include <Windows.h>

extern FARPROC(WINAPI* GetProcAddressO)(HMODULE hModule, LPCSTR lpProcName);

namespace mmfshooks {
static void* mod;

template <typename T> static void load_ord(T*& func_ptr, int ord) {
    func_ptr = reinterpret_cast<T*>(
        GetProcAddressO(reinterpret_cast<HMODULE>(mod), reinterpret_cast<LPCSTR>(ord)));
    ENSURE(func_ptr != nullptr);
}
} // namespace mmfshooks

void mmfshooks::init(void* _mod) {
    ASS(_mod != nullptr);
    mod = _mod;
    static int(__stdcall * GetBuildNumber)(void);
    load_ord(GetBuildNumber, 11);
    of::info("MMF2 dll build number: {}", GetBuildNumber() & 0xffff);
}
