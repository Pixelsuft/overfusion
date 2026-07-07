#include "gamehooks.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "customwindow.hpp"
#include "extrahooks.hpp"
#include "files.hpp"
#include "input.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "randhooks.hpp"
#include "state.hpp"
#include "threadhooks.hpp"
#include "timehooks.hpp"
#include "video.hpp"
#include "winhooks.hpp"
#include <Windows.h>

static void(__stdcall* RenderFrame)();
static void(__stdcall* RenderTransition)();

static bool check_already_processed() {
    auto& cfg = conf::get();
    if (cfg.render_type == conf::RenderType::GDI || cfg.render_type == conf::RenderType::D3D9) {
        if (!cfg.already_processed_frame) {
            cfg.already_processed_frame = true;
            of::warn("Frame drawing was skipped on frame {}", state::get_frame_counter());
            return true;
        }
    }
    return false;
}

static std::pair<int*, int*> prepare_and_get_pointers() {
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    ASS(pState != nullptr);
    auto pStep = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSubTickStep, pState));
    auto pIsPaused = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PIsPaused, pState));
    ASS(pStep != nullptr);
    ASS(pIsPaused != nullptr);
    *pIsPaused = false;
    *pStep = 1;
    return {pIsPaused, pStep};
}

static int(__stdcall* ProcessTransitionO)();
static int __stdcall ProcessTransitionH() {
    // Oh fuck another code dup
    auto& cfg = conf::get();
    state::early_update();
    if (!cfg.processing_save)
        input::process_update();
    state::before_update(true);
    auto pIsPaused = prepare_and_get_pointers().first;
    int ret;
    if (cfg.boxed_mode)
        cfg.is_paused = false;
    if (cfg.is_paused && !cfg.need_advance) {
        *pIsPaused = true;
        ret = ProcessTransitionO();
        if (cfg.custom_window)
            customwindow::render();
        if (RenderTransition)
            RenderTransition();
    } else {
        *pIsPaused = false;
        cfg.need_advance = false;
        cfg.processing_frame = true;
        cfg.already_processed_frame = false;
        ret = ProcessTransitionO();
        if (check_already_processed() && RenderTransition && cfg.redraw_on_skip)
            RenderTransition();
        cfg.processing_frame = false;
        video::after_draw();
        if (cfg.custom_window)
            customwindow::render();
        cfg.delayed_d3d9_present_hook = false;
    }
    state::after_update();
    // of::debug("Transition {}", ret);
    return ret;
}

static int(__stdcall* UpdateGameFrameO)();
static int __stdcall UpdateGameFrameH() {
    static bool inited = false;
    auto& cfg = conf::get();
    if (!inited) {
        of::info("UpdateGameFrame first call");
        if (cfg.custom_window) {
            of::info("Initializing custom window");
            auto prev_thread_disable = cfg.disable_threads;
            cfg.disable_threads = false;
            if (!customwindow::init()) {
                of::error("Failed to initialize custom window");
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
        randhooks::init();
        files::hook_fs();
        if (!plug::get().update_init()) {
            of::error("Failed to init plugin first-frame");
            // Exit???
            ENSURE(false);
        }
        hook::enable();
        hook::patch_iat();
    }
    plug::get().early_update();
    state::early_update();
    if (!cfg.processing_save)
        input::process_update();
    auto ptrs = prepare_and_get_pointers();
    int ret;
    if (!state::before_update(false) && !cfg.boxed_mode) {
        *ptrs.first = true; // pIsPaused
        ret = UpdateGameFrameO();
        if (cfg.custom_window)
            customwindow::render();
        if (RenderFrame)
            RenderFrame();
    } else {
        *ptrs.first = false; // pIsPaused
        cfg.need_advance = false;
        cfg.processing_frame = true;
        cfg.already_processed_frame = false;
        ret = UpdateGameFrameO();
        // Breaks FNAF magazine transition
        if (check_already_processed() && RenderFrame && cfg.redraw_on_skip)
            RenderFrame();
        cfg.processing_frame = false;
        video::after_draw();
        if (cfg.custom_window)
            customwindow::render();
        cfg.delayed_d3d9_present_hook = false;
        // pStep check
        if (*ptrs.second != 0) {
            of::warn("Subtick step check failed: got {} instead of 0", *ptrs.second);
            *ptrs.second = 0;
        }
    }
    if (ret != 0) {
        of::debug("UpdateGameFrame got ret {} on frame {}", ret, state::get_frame_counter());
        if (cfg.pause_on_scene_switch && state::get_frame_counter() != 0)
            cfg.is_paused = true;
    }
    state::after_update();
    return ret;
}

void gamehooks::init() {
    auto& cfg = conf::get();
    cfg.processing_frame = false;
    cfg.already_processed_frame = false;
    RenderFrame = reinterpret_cast<decltype(RenderFrame)>(cfg.pRenderFrame);
    RenderTransition = reinterpret_cast<decltype(RenderTransition)>(cfg.pRenderTransition);
    if (cfg.pUpdateGameFrame == nullptr)
        of::error("UpdateGameFrame was not hooked");
    else
        hook::hook(cfg.pUpdateGameFrame, UpdateGameFrameH, &UpdateGameFrameO);
    if (cfg.pProcessTransition == nullptr)
        of::error("ProcessTransition was not hooked");
    else
        hook::hook(cfg.pProcessTransition, ProcessTransitionH, &ProcessTransitionO);
    if (RenderFrame == nullptr)
        of::error("RenderFrame was not loaded");
    if (RenderTransition == nullptr)
        of::error("RenderTransition was not loaded");
}
