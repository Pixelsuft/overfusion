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
#include "video.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

static void(__stdcall* RenderFrame)();
static void(__stdcall* RenderTransition)();

static int(__stdcall* ProcessTransitionO)();
static int __stdcall ProcessTransitionH() {
    // Oh fuck another code dup
    auto& cfg = conf::get();
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    ASS(pState != nullptr);
    input::process_update();
    state::early_update();
    state::before_update(true);
    int ret;
    if (cfg.boxed_mode)
        cfg.is_paused = false;
    if (cfg.is_paused && !cfg.need_advance) {
        ret = ProcessTransitionO();
        if (cfg.custom_window)
            customwindow::render();
        if (RenderTransition)
            RenderTransition();
    } else {
        cfg.need_advance = false;
        video::set_allow_frame(true);
        ret = ProcessTransitionO();
        video::set_allow_frame(false);
        video::after_draw();
        if (cfg.custom_window)
            customwindow::render();
    }
    state::after_update();
    // spdlog::debug("Transition {}", ret);
    return ret;
}

static int(__stdcall* UpdateGameFrameO)();
static int __stdcall UpdateGameFrameH() {
    static bool inited = false;
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
        video::init();
        timehooks::update_init();
        threadhooks::update_init();
        extrahooks::init_adv();
        if (conf::get().virtual_fs)
            files::hook_fs();
        if (!plug::get().update_init()) {
            spdlog::error("Failed to init plugin first-frame");
            // Exit???
        }
        hook::enable();
        hook::patch_iat();
    }
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    ASS(pState != nullptr);
    state::early_update();
    auto pStep = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSubTickStep, pState));
    auto pIsPaused = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PIsPaused, pState));
    ASS(pStep != nullptr);
    ASS(pIsPaused != nullptr);
    *pIsPaused = false;
    *pStep = 1;
    if (!state::is_processing_save())
        input::process_update();
    int ret;
    if (!state::before_update(false) && !cfg.boxed_mode) {
        *pIsPaused = true;
        ret = UpdateGameFrameO();
        if (cfg.custom_window)
            customwindow::render();
        if (RenderFrame)
            RenderFrame();
    } else {
        cfg.need_advance = false;
        *pIsPaused = false;
        video::set_allow_frame(true);
        ret = UpdateGameFrameO();
        video::set_allow_frame(false);
        video::after_draw();
        if (cfg.custom_window)
            customwindow::render();
    }
    if (ret != 0)
        spdlog::debug("UpdateGameFrame got ret={}", ret);
    state::after_update();
    return ret;
}

void gamehooks::init() {
    auto& cfg = conf::get();
    RenderFrame = reinterpret_cast<decltype(RenderFrame)>(cfg.pRenderFrame);
    RenderTransition = reinterpret_cast<decltype(RenderTransition)>(cfg.pRenderTransition);
    if (cfg.pUpdateGameFrame == nullptr)
        spdlog::error("UpdateGameFrame was not hooked");
    else
        hook::hook(cfg.pUpdateGameFrame, UpdateGameFrameH, &UpdateGameFrameO);
    if (cfg.pProcessTransition == nullptr)
        spdlog::error("ProcessTransition was not hooked");
    else
        hook::hook(cfg.pProcessTransition, ProcessTransitionH, &ProcessTransitionO);
    if (RenderFrame == nullptr)
        spdlog::error("RenderFrame was not loaded");
    if (RenderTransition == nullptr)
        spdlog::error("RenderTransition was not loaded");
}
