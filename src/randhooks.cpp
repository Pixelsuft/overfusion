#define WIN32_LEAN_AND_MEAN
#include "randhooks.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "state.hpp"
#include <Windows.h>

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
    // Taken from decompiled msvcrt.dll (Windows XP - Windows 11)
    randhooks::rand_seed = randhooks::rand_seed * 0x343fd + 0x269ec3;
    return state::fetch_random_number(0x7fff, randhooks::rand_seed >> 16 & 0x7fff);
}

static errno_t(__cdecl* rand_sO)(unsigned int* randomValue);
static errno_t __cdecl rand_sH(unsigned int* randomValue) {
    if (randhooks::rand_thread != GetCurrentThreadId()) {
        of::debug("rand_s() was called from a different thread");
        return rand_sO(randomValue);
    }
    of::error("TODO: implement rand_s()");
    auto ret = rand_sO(randomValue);
    if (ret == 0 && randomValue)
        *randomValue = 0;
    return ret;
}

void randhooks::reset() { rand_seed = 1; }

void randhooks::init() {
    rand_thread = GetCurrentThreadId();
    reset();
    IAT_AUTO("msvcrt.dll", srand);
    IAT_AUTO("msvcrt.dll", rand);
    IAT_AUTO("msvcrt.dll", rand_s);
}
