#include "gamehooks.hpp"
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

static void(__stdcall* ProcessFrameRendering)();
static void(__stdcall* RenderTransition)();

static bool need_skip = false;

static int(__stdcall* ProcessTransitionO)();
static int __stdcall ProcessTransitionH() {
    // Oh fuck another code dup
    auto& cfg = conf::get();
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    if (pState == nullptr) {
        spdlog::warn("pState is nullptr");
        return ProcessTransitionO();
    }
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
        if (RenderTransition)
            RenderTransition();
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
    if (pState == nullptr) {
        spdlog::warn("pState is nullptr");
        return UpdateGameFrameO();
    }
    input::process_update();
    state::early_update();
    // Assuming they are not nullptrs
    auto& pStep =
        *reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSubTickStep, pState));
    auto& pIsPaused =
        *reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PIsPaused, pState));
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
        if (cfg.custom_window)
            customwindow::render();
        if (ProcessFrameRendering)
            ProcessFrameRendering();
    } else {
        cfg.need_advance = false;
        pIsPaused = false;
        ret = UpdateGameFrameO();
        // TODO: hook ProcessFrameRendering to inject custom window render here
        if (cfg.custom_window)
            customwindow::render();
    }
    if (ret == 3) {
        spdlog::debug("Scene change");
        need_skip = true;
    } else {
        // spdlog::debug("After update change");
        state::after_update();
    }
    return ret;
}

void gamehooks::init() {
    // TODO: render ONLY in ProcessFrameRendering to avoid time issues
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
    ProcessFrameRendering = reinterpret_cast<decltype(ProcessFrameRendering)>(
        plug::get().get_prop(plug::PtrProp::Render));
    if (ProcessFrameRendering == nullptr)
        spdlog::error("ProcessFrameRendering was not loaded");
    RenderTransition = reinterpret_cast<decltype(RenderTransition)>(
        plug::get().get_prop(plug::PtrProp::RenderTransition));
    if (RenderTransition == nullptr)
        spdlog::error("RenderTransition was not loaded");
}
