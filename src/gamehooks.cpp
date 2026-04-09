#include "gamehooks.hpp"
#include "config.hpp"
#include "customwindow.hpp"
#include "extrahooks.hpp"
#include "ass.hpp"
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

static void(__stdcall* RenderFrame)() = nullptr;
static void(__stdcall* RenderTransition)() = nullptr;

static bool need_skip = false;

static int(__stdcall* ProcessTransitionO)() = nullptr;
static int __stdcall ProcessTransitionH() {
    // Oh fuck another code dup
    auto& cfg = conf::get();
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    ASS(pState != nullptr);
    input::process_update();
    state::early_update();
    // TODO: check for skip
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

static int(__stdcall* UpdateGameFrameO)() = nullptr;
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
    *pIsPaused = false;
    if (need_skip) {
        need_skip = false;
        auto ret = UpdateGameFrameO();
        // Hacky way to skip waiting
        auto prev_skip = cfg.fast_forward;
        cfg.fast_forward = true;
        state::after_update();
        cfg.fast_forward = false;
        return ret;
    }
    state::before_update();
    int ret;
    if (cfg.is_paused && !cfg.need_advance) {
        *pIsPaused = true;
        ret = UpdateGameFrameO();
        if (cfg.custom_window)
            customwindow::render();
        if (RenderFrame)
            RenderFrame();
    } else {
        auto pTask = reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameTask, pState));
        ASS(pTask != nullptr);
        cfg.need_advance = false;
        *pIsPaused = false;
        ret = UpdateGameFrameO();
        if (cfg.custom_window)
            customwindow::render();
        // FIXME: better way to check if skip is needed
        // FIXME: IWBTG seems to be requires cheking it directly in ProcessFrameRendering
        need_skip = *pTask != 0;
    }
    if (!need_skip) {
        // spdlog::debug("After update change");
        state::after_update();
    } else {
        spdlog::debug("Scene change");
    }
    return ret;
}

void gamehooks::init() {
    if (UpdateGameFrameO == nullptr)
        spdlog::error("UpdateGameFrame was not hooked");
    if (ProcessTransitionO == nullptr)
        spdlog::error("ProcessTransition was not hooked");
    if (RenderFrame == nullptr)
        spdlog::error("RenderFrame was not loaded");
    if (RenderTransition == nullptr)
        spdlog::error("RenderTransition was not loaded");
}

void gamehooks::hook_update_func(void* ptr) {
    hook::hook(ptr, UpdateGameFrameH, &UpdateGameFrameO);
}

void gamehooks::set_render_func(void* ptr) {
    RenderFrame = reinterpret_cast<decltype(RenderFrame)>(ptr);
}

void gamehooks::hook_trans_update_func(void* ptr) {
    hook::hook(ptr, ProcessTransitionH, &ProcessTransitionO);
}

void gamehooks::set_trans_render_func(void* ptr) {
    RenderTransition = reinterpret_cast<decltype(RenderTransition)>(ptr);
}
