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
        unicode = true;
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
        return true;
    }

    bool update_init() override { return true; }

    std::optional<std::string> before_dll_load(string_view path, string_view fn) override {
        if (0 && fn == "wininet.dll")
            return "";
        // spdlog::info("Before load {}", fn);
        return {};
    }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        if (fn == "mmfs2.dll") {
        } else if (fn == "Lacewing.mfx") {
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

    bool get_bool_prop(plug::BoolProp prop) override {
        switch (prop) {
        case plug::BoolProp::NeedKeyMsg:
            return true;
        default:
            return false;
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
