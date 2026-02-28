#define WIN32_LEAN_AND_MEAN
#include "extrahooks.hpp"
#include "filehooks.hpp"
#include "input.hpp"
#include "loadhooks.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "timehooks.hpp"
#include "gamehooks.hpp"
#include "ui.hpp"
#include "config.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

static void of_main() {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
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
    filehooks::pre_init();
    conf::get().read();
    if (!plug::search_and_run())
        mem::terminate();
    if (!plug::get().pre_init()) {
        spdlog::error("Failed to init plugin: {}", plug::get().name);
        return;
    }
    timehooks::init();
    state::init();
    loadhook::init();
    winhooks::init();
    filehooks::init();
    input::init();
    extrahooks::init();
    gamehooks::init();
    hook::enable();
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
