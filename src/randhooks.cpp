#include "randhooks.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "state.hpp"

// Who needs thread safety, lul

namespace randhooks {
static unsigned int rand_seed;
}

static void __cdecl srandH(unsigned int _Seed) {
    of::debug("rand() seed {} -> {}", randhooks::rand_seed, _Seed);
    randhooks::rand_seed = _Seed;
}

static int __cdecl randH(void) {
    // Taken from decompiled msvcrt.dll (Windows 7 - Windows 11, at least)
    randhooks::rand_seed = randhooks::rand_seed * 0x343fd + 0x269ec3;
    int ret = randhooks::rand_seed >> 16 & 0x7fff;
    ret = state::fetch_random_number(0x7fff, ret);
    return ret;
}

void randhooks::init() {
    if (mem::get_base("msvcrt.dll") != 0) {
        rand_seed = 1;
        IAT_ONLY("msvcrt.dll", srand);
        IAT_ONLY("msvcrt.dll", rand);
    } else
        of::warn("Failed to hook rand() and srand() (msvcrt.dll was not loaded)");
}
