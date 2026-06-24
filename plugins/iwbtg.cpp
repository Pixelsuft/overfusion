#define WIN32_LEAN_AND_MEAN
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/ofs.hpp"
#include "../src/plugbase.hpp"
#include "../src/state.hpp"
#include <Windows.h>
#include "../src/log.hpp"

using ost::optional;
using ost::string_view;
using std::string;

class PlugIwbtg final : public plug::PlugBase {
public:
    PlugIwbtg() {
        name = "I Wanna Be The Guy";
        cmdline_append = string(" /SF \"") + string(ofs::get_cwd()) + "\\iwbtg.exe\" /SO94208";
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 50;
        cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x2bf30);
        cfg.pRenderFrame = reinterpret_cast<void*>(mem::get_base() + 0x17290);
        cfg.pProcessTransition = reinterpret_cast<void*>(mem::get_base() + 0x142e0);
        cfg.pRenderTransition = reinterpret_cast<void*>(mem::get_base() + 0x13e70);
        cfg.save_game_state = false; // Not supported
        // Idk but this fixes switching between scenes
        mem::write(mem::get_base() + 0x15af3, {0x90, 0x90});
        // No extra time logic
        mem::write(mem::get_base() + 0x2803, {0xeb});
        mem::write(mem::get_base() + 0x2824, {0x90, 0x90});
        mem::write(mem::get_base() + 0x31891, {0xeb});
        // Window title patch
        mem::write(mem::get_base() + 0x13123, {0xeb});
        mem::write(mem::get_base() + 0x13143, {0x90, 0x90});
        mem::write(mem::get_base() + 0x1314a, {0x90, 0x90});
        mem::write(mem::get_base() + 0x1314f, {0x90, 0x90});
        // No FPS limit/subtick fixes
        mem::write(mem::get_base() + 0x1604d, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // mem::write(mem::get_base() + 0x16508, {0xeb});
        return true;
    }

    bool update_init() override { return true; }

    optional<std::string> before_dll_load(string_view path, string_view fn) override {
        // of::info("Before load {}", fn);
        return {};
    }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        size_t base = reinterpret_cast<size_t>(mod);
        if (fn == "mmfs2.dll") {
            // I don't know why this is needed
            mem::write(base + 0x78d7, {0xeb});
            mem::write(base + 0x6e2f, {0xeb});
            mem::write(base + 0x6e80, {0x90, 0x90, 0x90, 0x90});
            mem::write(base + 0xbfb8, {0xeb});
        } else if (fn == "CCTrans.dll") {
            // Disable transitions
            // mem::write(base + 0x7448, {0xeb});
        }
    };

    void* get_prop(plug::PtrProp prop, void* data) override {
        switch (prop) {
        case plug::PtrProp::PState:
            return *reinterpret_cast<void**>(mem::get_base() + 0x48384);
        case plug::PtrProp::PStats:
            return nullptr;
        case plug::PtrProp::PGlobalApp:
            return *reinterpret_cast<void**>(mem::get_base() + 0x4837c);
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
        case plug::PtrProp::PRandomSeed:
            // From pState
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x18c);
        case plug::PtrProp::PSceneID:
            // From pGlobalApp
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x1f0);
        default:
            return nullptr;
        }
    }

    ost::expected<void, string> save_state(ofs::File& file) override {
        // I think that is unsupported
        if (conf::get().save_game_state)
            state::invalidate_process("Unsupported");
        return {};
    }

    ost::expected<void, string> load_state(ofs::File& file) override {
        if (!conf::get().is_replay)
            state::invalidate_process("Unsupported");
        return {};
    }

    static ost::optional<PlugIwbtg*> on_plugin_check() {
        if (mem::exe_name == "stdrt.exe" &&
            ofs::file_exists(string(ofs::get_cwd()) + "\\iwbtg.exe"))
            return new PlugIwbtg;
        return {};
    }
};

PLUG_REG(PlugIwbtg);
