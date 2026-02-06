#include "gamehooks.hpp"
#include "ass.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "input.hpp"
#include "state.hpp"
#include "config.hpp"
#include "timehooks.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

static bool MyKeyState(int k) { return GetKeyState(k) & 128; }

static void (__stdcall* ProcessFrameRendering)(void);

static int(__stdcall* UpdateGameFrameO)();
static int __stdcall UpdateGameFrameH() {
    static bool inited = false;
    static bool need_skip = false;
    if (!inited) {
        inited = true;
        winhooks::after_ui_init();
        timehooks::update_init();
        plug::get().update_init();
        hook::enable();
    }
    input::process_update();
    state::early_update();
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    if (pState == nullptr)
        return UpdateGameFrameO();
    auto& cfg = conf::get();
    auto& pStep = *reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSubTickStep, pState));
    auto& pIsPaused = *reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PIsPaused, pState));
    pStep = 1;
    if (need_skip) {
        need_skip = false;
        auto ret = UpdateGameFrameO();
        state::after_update();
        return ret;
    }
    state::before_update();
    int ret;
    if (cfg.is_paused && !cfg.need_advance) {
        ret = UpdateGameFrameO();
        pIsPaused = true;
        if (ProcessFrameRendering)
            ProcessFrameRendering();
    }
    else {
        cfg.need_advance = false;
        pIsPaused = false;
        ret = UpdateGameFrameO();
    }
    if (ret == 3) {
        spdlog::debug("Scene change");
        need_skip = true;
    }
    else {
        // spdlog::debug("After update change");
        state::after_update();
    }
    return ret;
}

void gamehooks::init() {
    auto temp_ptr = plug::get().get_prop(plug::PtrProp::Update);
    if (temp_ptr == nullptr)
        spdlog::error("UpdateGameFrame was not hooked");
    else
        hook::hook(temp_ptr, UpdateGameFrameH, &UpdateGameFrameO);
    ProcessFrameRendering = reinterpret_cast<decltype(ProcessFrameRendering)>(plug::get().get_prop(plug::PtrProp::Render));
    if (ProcessFrameRendering == nullptr)
        spdlog::error("ProcessFrameRendering was not loaded");
}
