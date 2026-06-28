#define WIN32_LEAN_AND_MEAN
#include "config.hpp"
#include "extrahooks.hpp"
#include "files.hpp"
#include "gamehooks.hpp"
#include "input.hpp"
#include "loadhooks.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "threadhooks.hpp"
#include "timehooks.hpp"
#include "ui.hpp"
#include "winhooks.hpp"
#include <Windows.h>

static void of_main() {
    // This function runs as early as possible
    AllocConsole();
    freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
#ifndef NO_SPDLOG
#ifdef _DEBUG
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
#else
    spdlog::set_pattern("[%^%l%$] %v");
#endif
#endif
    of::info("OverFusion injected!");
    SetEnvironmentVariableW(L"WINEDEBUG", L"+d3d,+d3d9,+wined3d");
    SetEnvironmentVariableW(L"WINE_DBG_OUTPUT", L"stdout");
    SetEnvironmentVariableW(L"GALLIUM_DRIVER", L"llvmpipe");
    SetEnvironmentVariableW(L"LIBGL_ALWAYS_SOFTWARE", L"true");
    conf::init();
    if (conf::get().project_name.empty())
        return;
    mem::init();
    ui::init();
    ofs::pre_init();
    files::pre_init();
    conf::get().read();
    of::debug("Base address: {}", reinterpret_cast<void*>(mem::get_base()));
    if (conf::get().wait_for_debugger) {
        while (!IsDebuggerPresent())
            Sleep(500);
    }
    if (!plug::search_and_run())
        return;
    if (!plug::get().pre_init()) {
        of::error("Failed to init plugin: {}", plug::get().name);
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
