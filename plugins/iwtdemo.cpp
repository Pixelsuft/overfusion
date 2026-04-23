#define WIN32_LEAN_AND_MEAN
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ost::optional;
using ost::string_view;
using std::string;

class PlugIwtDemo final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);

public:
    PlugIwtDemo() {
        name = "I WANNA TRY - A New Adventure Demo";
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
        cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x462e0);
        cfg.pRenderFrame = reinterpret_cast<void*>(mem::get_base() + 0x2c3f0);
        cfg.pProcessTransition = reinterpret_cast<void*>(mem::get_base() + 0x28b50);
        cfg.pRenderTransition = reinterpret_cast<void*>(mem::get_base() + 0x29f30);
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
        // Fix crash VERY EXPERIMENTAL
        mem::write(mem::get_base() + 0x493fd, {0xeb});
        return true;
    }

    bool update_init() override {
        auto& cfg = conf::get();
        cfg.gApp = *reinterpret_cast<void**>(mem::get_base() + 0xb49d0);
        return true;
    }

    optional<std::string> before_dll_load(string_view path, string_view fn) override {
        if (fn == "wininet.dll")
            return "";
        // spdlog::info("Before load {}", fn);
        return {};
    }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        if (fn == "mmfs2.dll") {
            // No __security_check_cookie
            mem::write(reinterpret_cast<size_t>(mod) + 0x483cc,
                       {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
            // Somehow fixes game state save
            // mem::write(reinterpret_cast<size_t>(mod) + 0x14928, {0x90, 0x90});
        } else if (fn == "Lacewing.mfx") {
            // No theading stuff
            mem::write(reinterpret_cast<size_t>(mod) + 0xb209, {0xeb});
        } else if (fn == "Download.mfx") {
            // No crash when blocking wininet.dll
            mem::write(reinterpret_cast<size_t>(mod) + 0x16f9, {0xeb});
            mem::write(reinterpret_cast<size_t>(mod) + 0x1727, {0x90, 0x90});
        } else if (fn == "ZipObject.mfx") {
            // No random
            mem::write(reinterpret_cast<size_t>(mod) + 0x8eb7, {0xeb});
            mem::write(reinterpret_cast<size_t>(mod) + 0x8ed3,
                       {0x31, 0xc0, 0x90, 0x90, 0x90, 0x90});
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
        default:
            return nullptr;
        }
    }

    ost::expected<void, string> save_state(ofs::File& file) override {
        // Replicating game engine mechanics
        // TODO: reinterpret cast
        if (conf::get().save_game_state) {
            size_t ptr = *(size_t*)(mem::get_base() + 0xb49d4);
            *(short*)(ptr + 0x436) = 0;
            SaveGameState(file.get_handle());
        }
        return {};
    }

    ost::expected<void, string> load_state(ofs::File& file) override {
        if (!conf::get().is_replay) {
            unsigned int outframe = 0;
            size_t ptr = *(size_t*)(mem::get_base() + 0xb49d4);
            *(short*)(ptr + 0x30) = 0;
            LoadGameState(file.get_handle(), &outframe);
        }
        return {};
    }
};

static void on_plugin_check(plug::PlugBase** buf, bool& check) {
    if (buf) {
        *buf = new PlugIwtDemo;
    } else {
        check = mem::exe_name == "I WANNA TRY - A New Adventure Demo.exe";
    }
}

PLUG_REG(PlugIwtDemo, on_plugin_check)
