#include "gamehooks.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "customwindow.hpp"
#include "extrahooks.hpp"
#include "files.hpp"
#include "input.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "threadhooks.hpp"
#include "timehooks.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

static bool need_skip = false;

static void(__stdcall* RenderTransitionO)();
static void __stdcall RenderTransitionH() {
    auto& cfg = conf::get();
    if (cfg.custom_window)
        customwindow::render();
    if (!need_skip)
        RenderTransitionO();
}

static int(__stdcall* ProcessTransitionO)();
static int __stdcall ProcessTransitionH() {
    // Oh fuck another code dup
    auto& cfg = conf::get();
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    ASS(pState != nullptr);
    input::process_update();
    state::early_update();
    if (need_skip) {
        need_skip = false;
        auto ret = ProcessTransitionO();
        state::after_update();
        return ret;
    }
    state::before_update();
    int ret;
    if (cfg.is_paused && !cfg.need_advance) {
        ret = ProcessTransitionO();
        if (cfg.custom_window)
            customwindow::render();
        if (RenderTransitionO)
            RenderTransitionO();
    } else {
        cfg.need_advance = false;
        ret = ProcessTransitionO();
        if (cfg.custom_window)
            customwindow::render();
    }
    state::after_update();
    // spdlog::debug("Transition {}", ret);
    return ret;
}

static void(__stdcall* ProcessFrameRenderingO)();
static void __stdcall ProcessFrameRenderingH() {
    auto& cfg = conf::get();
    if (cfg.custom_window)
        customwindow::render();
    if (!need_skip)
        ProcessFrameRenderingO();
}

static int(__stdcall* UpdateGameFrameO)();
static int __stdcall UpdateGameFrameH() {
    static bool inited = false;
    static bool need_skip = false;
    auto& cfg = conf::get();
    if (!inited) {
        spdlog::info("UpdateGameFrame first call");
        if (cfg.custom_window) {
            spdlog::info("Initializing custom window for software renderer");
            auto prev_thread_disable = cfg.disable_threads;
            cfg.disable_threads = false;
            if (!customwindow::init()) {
                spdlog::error("Failed to initialize custom window");
                cfg.custom_window = false; // NO-OP
            }
            cfg.disable_threads = prev_thread_disable;
        }
        inited = true;
        winhooks::after_ui_init();
        timehooks::update_init();
        threadhooks::update_init();
        extrahooks::init_adv();
        if (conf::get().virtual_fs)
            files::hook_fs();
        plug::get().update_init();
        hook::enable();
    }
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    ASS(pState != nullptr);
    input::process_update();
    state::early_update();
    // Assuming they are not nullptrs
    auto pStep = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSubTickStep, pState));
    auto pIsPaused = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PIsPaused, pState));
    ASS(pStep != nullptr);
    ASS(pIsPaused != nullptr);
    *pStep = 1;
    if (need_skip) {
        need_skip = false;
        auto ret = UpdateGameFrameO();
        state::after_update();
        return ret;
    }
    *pIsPaused = false;
    state::before_update();
    int ret;
    if (cfg.is_paused && !cfg.need_advance) {
        *pIsPaused = true;
        ret = UpdateGameFrameO();
        ProcessFrameRenderingH();
    } else {
        cfg.need_advance = false;
        *pIsPaused = false;
        ret = UpdateGameFrameO();
    }
    if (ret == 0) {
        state::after_update();
    } else {
        // FIXME: proper scene change vs transition etc.
        spdlog::debug("Scene change");
        need_skip = true;
    }
    return ret;
}

void gamehooks::init() {
    ProcessFrameRenderingO = nullptr;
    RenderTransitionO = nullptr;
    auto temp_ptr = plug::get().get_prop(plug::PtrProp::Update);
    if (temp_ptr == nullptr)
        spdlog::error("UpdateGameFrame was not hooked");
    else
        hook::hook(temp_ptr, UpdateGameFrameH, &UpdateGameFrameO);
    temp_ptr = plug::get().get_prop(plug::PtrProp::ProcessTransition);
    if (temp_ptr == nullptr)
        spdlog::error("ProcessTransition was not hooked");
    else
        hook::hook(temp_ptr, ProcessTransitionH, &ProcessTransitionO);
    temp_ptr = plug::get().get_prop(plug::PtrProp::Render);
    if (temp_ptr == nullptr)
        spdlog::error("ProcessFrameRendering was not hooked");
    else
        hook::hook(temp_ptr, ProcessFrameRenderingH, &ProcessFrameRenderingO);
    temp_ptr = plug::get().get_prop(plug::PtrProp::RenderTransition);
    if (temp_ptr == nullptr)
        spdlog::error("RenderTransition was not hooked");
    else
        hook::hook(temp_ptr, RenderTransitionH, &RenderTransitionO);
}
