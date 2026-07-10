#define WIN32_LEAN_AND_MEAN
#include "mmfshooks.hpp"
#include "ass.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "uconv.hpp"
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

static HANDLE(__stdcall* Ordinal_420O)(LPCSTR lpFileName);
static HANDLE __stdcall Ordinal_420H(LPCSTR lpFileName) {
    of::debug("Ordinal_420: {}", uconv::from_ansi(lpFileName));
    return Ordinal_420O(lpFileName);
}

static HANDLE(__stdcall* Ordinal_424O)(LPCSTR lpFileName, int flags);
static HANDLE __stdcall Ordinal_424H(LPCSTR lpFileName, int flags) {
    of::debug("Ordinal_424: {} {}", uconv::from_ansi(lpFileName), flags);
    return Ordinal_424O(lpFileName, flags);
}

void mmfshooks::init(void* _mod) {
    ASS(_mod != nullptr);
    mod = _mod;
    int(__stdcall * GetBuildNumber)(void);
    load_ord(GetBuildNumber, 11);
    of::info("MMF2 dll build number: {}", GetBuildNumber() & 0xffff);
    if (false) {
        IAT_ORD_AUTO("mmfs2.dll", 420);
        IAT_ORD_AUTO("mmfs2.dll", 424);
    }
}
