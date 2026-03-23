#include "gamehooks.hpp"
#include "ass.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "input.hpp"
#include "state.hpp"
#include "files.hpp"
#include "config.hpp"
#include "timehooks.hpp"
#include "extrahooks.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

static void (__stdcall* ProcessFrameRendering)(void);

static int(__stdcall* UpdateGameFrameO)();
static int __stdcall UpdateGameFrameH() {
    static bool inited = false;
    static bool need_skip = false;
    if (!inited) {
        spdlog::debug("UpdateGameFrame first call");
        inited = true;
        winhooks::after_ui_init();
        timehooks::update_init();
        extrahooks::init_adv();
        files::hook_fs();
        plug::get().update_init();
        hook::enable();
    }
    // TODO
    spdlog::debug("update");
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    if (pState == nullptr) {
        spdlog::warn("pState is nullptr");
        return UpdateGameFrameO();
    }
    input::process_update();
    state::early_update();
    auto& cfg = conf::get();
    // Assuming they are not nullptrs
    auto& pStep = *reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSubTickStep, pState));
    auto& pIsPaused = *reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PIsPaused, pState));
    pStep = 1;
    if (need_skip) {
        need_skip = false;
        auto ret = UpdateGameFrameO();
        state::after_update();
        return ret;
    }
    pIsPaused = false;
    state::before_update();
    int ret;
    if (cfg.is_paused && !cfg.need_advance) {
        pIsPaused = true;
        ret = UpdateGameFrameO();
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
