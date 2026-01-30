#include "gamehooks.hpp"
#include "ass.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "timehooks.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

static bool MyKeyState(int k) { return GetKeyState(k) & 128; }

static int(__stdcall* ProcessUpdateO)();
static int __stdcall ProcessUpdateH() {
    static bool inited = false;
    static bool need_skip = false;
    if (!inited) {
        inited = true;
        plug::get().update_init();
        winhooks::after_ui_init();
        timehooks::update_init();
        hook::enable();
    }
    auto pState = plug::get().get_prop(plug::PtrProp::PState);
    auto pStep = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PSubTickStep, pState));
    *pStep = 1;
    if (need_skip) {
        need_skip = false;
        return ProcessUpdateO();
    }
    static bool f = false;
    if (MyKeyState('F')) {
        if (!f) {
            spdlog::info("Load");
            f = true;
            ofs::File file("test.bin", 0);
            state::load(file);
        }
    } else
        f = false;
    static bool g = false;
    if (MyKeyState('G')) {
        if (!g) {
            spdlog::info("Save");
            g = true;
            // ofs::File file("test.bin", 1);
            // state::save(file);
            auto isPaused = reinterpret_cast<int*>(plug::get().get_prop(plug::PtrProp::PIsPaused, pState));
            *isPaused = 1;
        }
    } else
        g = false;
    auto ret = ProcessUpdateO();
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
    ASS(temp_ptr != nullptr);
    if (temp_ptr != nullptr)
        hook::hook(temp_ptr, ProcessUpdateH, &ProcessUpdateO);
}
