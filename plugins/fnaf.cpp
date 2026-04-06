#define WIN32_LEAN_AND_MEAN
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ost::string_view, ost::optional;

class PlugFnaf final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);

public:
    PlugFnaf() {
        name = "Five Nights at Freddy's";
        need_key_message = false;
        SaveGameState = nullptr;
        LoadGameState = nullptr;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 60;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x47470);
        ASS(SaveGameState != nullptr);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x49060);
        ASS(LoadGameState != nullptr);
        // No waiting
        // mem::write(mem::get_base() + 0x2ea5, {0xeb});
        mem::write(mem::get_base() + 0x2f28, {0xeb});
        mem::write(mem::get_base() + 0x2f57, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // Use high precision timer instead of ugly SetTimer
        mem::write(mem::get_base() + 0x23b88, {0xeb});
        // By saying pause I mean pause
        mem::write(mem::get_base() + 0x5784b, {0xeb});
        mem::write(mem::get_base() + 0x57891, {0xeb});
        // Game FPS is fine
        mem::write(mem::get_base() + 0x29f7a, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // Game title
        mem::write(mem::get_base() + 0x268d8, {0x90, 0x90});
        mem::write(mem::get_base() + 0x268df, {0x90, 0x90});
        mem::write(mem::get_base() + 0x268e5, {0x90, 0x90});
        return true;
    }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        size_t base = reinterpret_cast<size_t>(mod);
        if (fn == "mmfs2.dll") {
        } else if (fn == "cctrans.dll") {
            // Disable transitions
            // mem::write(base + 0x78b8, {0xeb});
        } else if (fn == "Perspective.mfx") {
            // Patch for disabling Perspective
            // mem::write(base + 0x169d, {0x31, 0xc0, 0xc2, 0x04, 0x00});
        }
    };

    void* get_prop(plug::PtrProp prop, void* data) override {
        switch (prop) {
        case plug::PtrProp::PState:
            return *reinterpret_cast<void**>(mem::get_base() + 0xac9b4);
        case plug::PtrProp::PGlobalApp:
            return *reinterpret_cast<void**>(mem::get_base() + 0xac9ac);
        case plug::PtrProp::PNextFrameTask:
            // From pState
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x30);
        case plug::PtrProp::PNextFrameData:
            // From pState
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x38);
        case plug::PtrProp::PSubTickStep:
            // From pState
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x4d8);
        case plug::PtrProp::PIsPaused:
            // From pState
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x178);
        case plug::PtrProp::PSceneID:
            // From pGlobalApp
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x1f0);
        case plug::PtrProp::Update:
            return reinterpret_cast<void*>(mem::get_base() + 0x45430);
        case plug::PtrProp::Render:
            return reinterpret_cast<void*>(mem::get_base() + 0x2b970);
        case plug::PtrProp::ProcessTransition:
            return reinterpret_cast<void*>(mem::get_base() + 0x28060);
        case plug::PtrProp::RenderTransition:
            return reinterpret_cast<void*>(mem::get_base() + 0x29470);
        default:
            return nullptr;
        }
    }

    bool save_state(ofs::File& file) override {
        SaveGameState(file.get_handle());
        return true;
    }

    bool load_state(ofs::File& file) override {
        unsigned int outframe = 0;
        LoadGameState(file.get_handle(), &outframe);
        return true;
    }
};

static void on_plugin_check(plug::PlugBase** buf, bool& check) {
    if (buf) {
        *buf = new PlugFnaf;
    } else {
        check = mem::exe_name == "FiveNightsatFreddys.exe";
    }
}

PLUG_REG(PlugFnaf, on_plugin_check)
