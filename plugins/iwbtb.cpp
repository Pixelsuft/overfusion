#define WIN32_LEAN_AND_MEAN
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ost::string_view;

class PlugIwtDemo final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);

public:
    PlugIwtDemo() {
        name = "I Wanna Be The Boshy";
        unicode = false;
        need_key_message = false;
        SaveGameState = nullptr;
        LoadGameState = nullptr;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 50;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x37dc0);
        ASS(SaveGameState != nullptr);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x39780);
        ASS(LoadGameState != nullptr);
        // From boshyst
        mem::write(mem::get_base() + 0x4b74, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x4b6d, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x4c29, {0xeb});
        mem::write(mem::get_base() + 0x4659f, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x2a74, {0x90, 0x90});
        mem::write(mem::get_base() + 0x2a49, {0xeb});
        mem::write(mem::get_base() + 0x2a53, {0xeb});
        mem::write(mem::get_base() + 0x43036, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x4304e, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x43056, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x44e3, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0xb245, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x2013b,
                   {0x31, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                    0x90, 0x90, 0x90, 0x90, 0x85, 0xC0, 0xEB, 0x00});
        mem::write(mem::get_base() + 0x332df, {0xeb});
        mem::write(mem::get_base() + 0x3eaab, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x3ea96, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x1d93e, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x1d931, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x36241, {0x90, 0x90});
        mem::write(mem::get_base() + 0xe1fa, {0xeb});
        mem::write(mem::get_base() + 0xe2cd, {0xeb});
        mem::write(mem::get_base() + 0x41985, {0xeb});
        mem::write(mem::get_base() + 0x425a1, {0xeb});
        mem::write(mem::get_base() + 0x436b4, {0xeb});
        mem::write(mem::get_base() + 0x431a2, {0x66, 0xe9, 0xca});
        return true;
    }

    bool update_init() override { return true; }

    std::optional<std::string> before_dll_load(string_view path, string_view fn) override {
        if (fn == "wininet.dll")
            return "";
        // spdlog::info("Before load {}", fn);
        return {};
    }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        size_t base = reinterpret_cast<size_t>(mod);
        if (fn == "mmfs2.dll") {
            mem::write(base + 0xc3f3, {0x90, 0x90});
            mem::write(base + 0xc3a5, {0xeb});
            mem::write(base + 0x2dd58, {0xeb});
            mem::write(base + 0x4459f, {0xeb});
            mem::write(base + 0x41af4, {0xeb});
        } else if (fn == "mmf2d3d9.dll") {
            mem::write(base + 0x270c, {0xeb});
        } else if (fn == "Lacewing.mfx") {
            mem::write(base + 0x974f, {0xeb});
            mem::write(base + 0xb209, {0xeb});
            mem::write(base + 0x88cb, {0x90, 0x90, 0x90, 0x90, 0x90});
        }
    };

    void* get_prop(plug::PtrProp prop, void* data) override {
        switch (prop) {
        case plug::PtrProp::PState:
            return *reinterpret_cast<void**>(mem::get_base() + 0x59a9c);
        case plug::PtrProp::PGlobalApp:
            return *reinterpret_cast<void**>(mem::get_base() + 0x59a94);
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
            return reinterpret_cast<void*>(mem::get_base() + 0x365a0);
        case plug::PtrProp::Render:
            return reinterpret_cast<void*>(mem::get_base() + 0x1ebf0);
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
        *buf = new PlugIwtDemo;
    } else {
        check = mem::exe_name == "I Wanna Be The Boshy.exe";
    }
}

PLUG_REG(PlugIwtDemo, on_plugin_check)
