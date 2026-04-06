#define WIN32_LEAN_AND_MEAN
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include <spdlog/spdlog.h>

using ost::string_view;
using ost::optional;

class PlugIwtOld final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);

public:
    PlugIwtOld() {
        name = "I Wanna Try 1.9.8.3";
        need_key_message = true;
        SaveGameState = nullptr;
        LoadGameState = nullptr;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 60;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x48350);
        ASS(SaveGameState != nullptr);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x49f40);
        ASS(LoadGameState != nullptr);
        // Show scene name in title
        mem::write(mem::get_base() + 0x273a8, {0x90, 0x90});
        // No __security_check_cookie
        mem::write(mem::get_base() + 0x6d696, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // No window hooks (already patched but for 0.00001 fps boost)
        mem::write(mem::get_base() + 0x5a9d, {0xeb});
        // No internal input recording overhead
        mem::write(mem::get_base() + 0x2add4, {0x90, 0x90});
        mem::write(mem::get_base() + 0x2add7, {0xeb});
        // No waiting
        mem::write(mem::get_base() + 0x2ea5, {0xeb});
        mem::write(mem::get_base() + 0x2ee5, {0xeb});
        mem::write(mem::get_base() + 0x2f0a, {0x90, 0x90});
        // Use high precision timer instead of ugly SetTimer
        mem::write(mem::get_base() + 0x24618, {0xeb});
        // Save state fixes (experimental)
        mem::write(mem::get_base() + 0x483a3, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x49a5e, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x4835c, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                                               0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x58227, {0x66, 0xe9, 0x94, 0x00, 0x90, 0x90});
        // By saying pause I mean pause
        mem::write(mem::get_base() + 0x2aaf8, {0xeb});
        mem::write(mem::get_base() + 0x586ab, {0xeb});
        mem::write(mem::get_base() + 0x586f1, {0xeb});
        // No extra win32 event logic
        mem::write(mem::get_base() + 0x42176, {0xeb});
        // No hotkeys
        mem::write(mem::get_base() + 0x5162a, {0x31, 0xf6});
        // Game FPS is fine
        mem::write(mem::get_base() + 0x2ab70, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        return true;
    }

    bool update_init() override { return true; }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        if (fn == "mmfs2.dll") {
            // No __security_check_cookie
            mem::write(mem::get_base() + 0x6d696, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
            // Somehow fixes game state save
            mem::write(reinterpret_cast<size_t>(mod) + 0x14928, {0x90, 0x90});
        } else if (fn == "Lacewing.mfx") {
            // No theading stuff
            mem::write(reinterpret_cast<size_t>(mod) + 0xb209, {0xeb});
        }
    };

    void* get_prop(plug::PtrProp prop, void* data) override {
        switch (prop) {
        case plug::PtrProp::PState:
            return *reinterpret_cast<void**>(mem::get_base() + 0xb49d4);
        case plug::PtrProp::PGlobalApp:
            return *reinterpret_cast<void**>(mem::get_base() + 0xb49cc);
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
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x1ec);
        case plug::PtrProp::Update:
            return reinterpret_cast<void*>(mem::get_base() + 0x462e0);
        case plug::PtrProp::Render:
            return reinterpret_cast<void*>(mem::get_base() + 0x2c3f0);
        case plug::PtrProp::ProcessTransition:
            return reinterpret_cast<void*>(mem::get_base() + 0x28b50);
        case plug::PtrProp::RenderTransition:
            return reinterpret_cast<void*>(mem::get_base() + 0x29f30);
        default:
            return nullptr;
        }
    }

    bool save_state(ofs::File& file) override {
        // Replicating game engine mechanics
        size_t ptr = *(size_t*)(mem::get_base() + 0xb49d4);
        *(short*)(ptr + 0x436) = 0;
        SaveGameState(file.get_handle());
        return true;
    }

    bool load_state(ofs::File& file) override {
        unsigned int outframe = 0;
        size_t ptr = *(size_t*)(mem::get_base() + 0xb49d4);
        *(short*)(ptr + 0x30) = 0;
        LoadGameState(file.get_handle(), &outframe);
        return true;
    }
};

static void on_plugin_check(plug::PlugBase** buf, bool& check) {
    if (buf) {
        *buf = new PlugIwtOld;
    } else {
        check = mem::exe_name == "I WANNA TRY 1.9.8.3.exe";
    }
}

PLUG_REG(PlugIwtOld, on_plugin_check)
