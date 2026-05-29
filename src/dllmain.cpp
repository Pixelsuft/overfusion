#define WIN32_LEAN_AND_MEAN
#include "config.hpp"
#include "extrahooks.hpp"
#include "files.hpp"
#include "gamehooks.hpp"
#include "input.hpp"
#include "loadhooks.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "threadhooks.hpp"
#include "timehooks.hpp"
#include "ui.hpp"
#include "video.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

static void of_main() {
    // This function runs as early as possible
    AllocConsole();
    freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
#ifdef _DEBUG
    spdlog::set_level(spdlog::level::debug);
#endif
    spdlog::info("OverFusion injected!");
#if 0
    while (!IsDebuggerPresent())
        Sleep(500);
#endif
    conf::init();
    mem::init();
    ui::init();
    files::pre_init();
    conf::get().read();
    if (!plug::search_and_run())
        mem::terminate();
    if (!plug::get().pre_init()) {
        spdlog::error("Failed to init plugin: {}", plug::get().name);
        return;
    }
    timehooks::init();
    state::init();
    loadhooks::init();
    winhooks::init();
    files::init();
    input::init();
    extrahooks::init();
    gamehooks::init();
    threadhooks::pre_init();
    video::init();
    hook::enable();
    hook::patch_iat();
}

extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    DisableThreadLibraryCalls(hModule);
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        of_main();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
