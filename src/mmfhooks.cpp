#define WIN32_LEAN_AND_MEAN
#include "mmfhooks.hpp"
#include "ass.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "uconv.hpp"
#include <Windows.h>

extern FARPROC(WINAPI* GetProcAddressO)(HMODULE hModule, LPCSTR lpProcName);

namespace mmfhooks {
static void* mod;

template <typename T> static void load_ord(T*& func_ptr, int ord) {
    func_ptr = reinterpret_cast<T*>(
        GetProcAddressO(reinterpret_cast<HMODULE>(mod), reinterpret_cast<LPCSTR>(ord)));
    ENSURE(func_ptr != nullptr);
}
} // namespace mmfhooks

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

static LPVOID(__stdcall* CreateTransitionO)(DWORD dwTransID, DWORD dwMode, DWORD dwSpeed,
                                            LPVOID lpReserved);
static LPVOID __stdcall CreateTransitionH(DWORD dwTransID, DWORD dwMode, DWORD dwSpeed,
                                          LPVOID lpReserved) {
    auto ret = CreateTransitionO(dwTransID, dwMode, dwSpeed, lpReserved);
    of::debug("CreateTransition {} {} {} {}", dwTransID, dwMode, dwSpeed, lpReserved);
    return ret;
}

void* mmfhooks::cctrans_get_proc(of::string_view proc, void* ret) {
    if (proc == "CreateTransition") {
        CreateTransitionO = reinterpret_cast<decltype(CreateTransitionO)>(ret);
        return reinterpret_cast<void*>(CreateTransitionH);
    }
    return ret;
}

void mmfhooks::init(void* _mod) {
    ASS(_mod != nullptr);
    mod = _mod;
    int(__stdcall * GetBuildNumber)(void);
    load_ord(GetBuildNumber, 11);
    of::info("MMF2 dll build number: {}", GetBuildNumber() & 0xffff);
    if (false) {
        IAT_ORD_AUTO("mmfs2.dll", 420);
        IAT_ORD_AUTO("mmfs2.dll", 424);
    }
    IAT_AUTO("cctrans.dll", CreateTransition);
}
