#define WIN32_LEAN_AND_MEAN
#include "randhooks.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "state.hpp"
#include <Windows.h>

// TODO: patch TextInputFramework.dll to not use rand() and srand()

namespace randhooks {
static DWORD rand_thread;
static unsigned int rand_seed;
} // namespace randhooks

static void(__cdecl* srandO)(unsigned int _Seed);
static void __cdecl srandH(unsigned int _Seed) {
    if (randhooks::rand_thread != GetCurrentThreadId()) {
        of::debug("srand() was called from a different thread");
        return srandO(_Seed);
    }
    of::debug("srand() seed {} -> {}", randhooks::rand_seed, _Seed);
    randhooks::rand_seed = _Seed;
}

static int(__cdecl* randO)(void);
static int __cdecl randH(void) {
    if (randhooks::rand_thread != GetCurrentThreadId()) {
        of::debug("rand() was called from a different thread");
        return randO();
    }
    // Taken from decompiled msvcrt.dll (Windows 7 - Windows 11, at least)
    randhooks::rand_seed = randhooks::rand_seed * 0x343fd + 0x269ec3;
    int ret = randhooks::rand_seed >> 16 & 0x7fff;
    ret = state::fetch_random_number(0x7fff, ret);
    return ret;
}

static errno_t(__cdecl* rand_sO)(unsigned int* randomValue);
static errno_t __cdecl rand_sH(unsigned int* randomValue) {
    of::warn("Game used rand_s() which is not implemented");
    return rand_sO(randomValue);
}

void randhooks::reset() { rand_seed = 1; }

void randhooks::init() {
    if (mem::get_base("msvcrt.dll") != 0) {
        IAT_AUTO("msvcrt.dll", srand);
        IAT_AUTO("msvcrt.dll", rand);
        IAT_AUTO("msvcrt.dll", rand_s);
        rand_thread = GetCurrentThreadId();
    } else
        of::warn("Failed to hook rand() and srand() (msvcrt.dll was not loaded)");
    reset();
}
