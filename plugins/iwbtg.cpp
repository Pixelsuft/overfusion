#define WIN32_LEAN_AND_MEAN
#include "../src/config.hpp"
#include "../src/gamehooks.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ost::optional;
using ost::string_view;
using std::string;

class PlugIwbtg final : public plug::PlugBase {
public:
    PlugIwbtg() {
        name = "I Wanna Be The Guy";
        cmdline_append = " /SF \"E:\\Games\\IWBTG\\iwbtg.exe\" /SO94208";
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 50;
        gamehooks::hook_update_func(reinterpret_cast<void*>(mem::get_base() + 0x2bf30));
        gamehooks::set_render_func(reinterpret_cast<void*>(mem::get_base() + 0x17290));
        gamehooks::hook_trans_update_func(reinterpret_cast<void*>(mem::get_base() + 0x142e0));
        gamehooks::set_trans_render_func(reinterpret_cast<void*>(mem::get_base() + 0x13e70));
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
        // No FPS limit
        mem::write(mem::get_base() + 0x16049, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        return true;
    }

    bool update_init() override { return true; }

    optional<std::string> before_dll_load(string_view path, string_view fn) override {
        // spdlog::info("Before load {}", fn);
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
        } else if (fn == "CCTrans.dll") {
            // Disable transitions
            // mem::write(base + 0x7448, {0xeb});
        }
    };

    void* get_prop(plug::PtrProp prop, void* data) override {
        switch (prop) {
        case plug::PtrProp::PState:
            return *reinterpret_cast<void**>(mem::get_base() + 0x48384);
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
        case plug::PtrProp::PSceneID:
            // From pGlobalApp
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x1f0);
        default:
            return nullptr;
        }
    }

    ost::expected<void, string> save_state(ofs::File& file) override {
        // I think that is unsupported
        return ost::unexpected<string>("Not supported");
    }

    ost::expected<void, string> load_state(ofs::File& file) override {
        return ost::unexpected<string>("Not supported");
    }
};

static void on_plugin_check(plug::PlugBase** buf, bool& check) {
    if (buf) {
        *buf = new PlugIwbtg;
    } else {
        // FIXME
        check = mem::exe_name == "stdrt.exe";
    }
}

PLUG_REG(PlugIwbtg, on_plugin_check)
