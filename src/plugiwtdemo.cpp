#define WIN32_LEAN_AND_MEAN
#include "ass.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include <spdlog/spdlog.h>
#include <Windows.h>
// TODO: move to plugins dir from src

class PlugIwt : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);

public:
    PlugIwt() {
        name = "I WANNA TRY - A New Adventure Demo";
        fps = 60;
        unicode = true;
        SaveGameState = nullptr;
        LoadGameState = nullptr;
    }

    void pre_init() override {
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x48350);
        ASS(SaveGameState != nullptr);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x49f40);
        ASS(LoadGameState != nullptr);
        // Force /DEBUG (window title)
        // *(int*)(mem::get_base() + 0xb4b48) = 1;
        // Show scene name in title
        // mem::write(mem::get_base() + 0x273a8, {0x90, 0x90});
        // No __security_check_cookie
        // mem::write(mem::get_base() + 0x6d696, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // No window hooks (already patched but for 0.00001 fps boost)
        // mem::write(mem::get_base() + 0x5a9d, {0xeb});
        // No internal input recording overhead
        // mem::write(mem::get_base() + 0x2add4, {0x90, 0x90});
        // mem::write(mem::get_base() + 0x2add7, {0xeb});
        // No waiting
        // mem::write(mem::get_base() + 0x2ee5, {0xeb});
        // Use high precision timer instead of ugly SetTimer
        // mem::write(mem::get_base() + 0x24618, {0xeb});
    }

    void update_init() override {}

    void after_dll_load(std::string_view path, std::string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        if (fn == "mmfs2.dll") {
            // No __security_check_cookie
            // mem::write(mem::get_base() + 0x6d696, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
            // Somehow fixes game state save
            // mem::write(reinterpret_cast<size_t>(mod) + 0x14928, {0x90, 0x90});
        } else if (fn == "Lacewing.mfx") {
            // No theading stuff
            // mem::write(reinterpret_cast<size_t>(mod) + 0xb209, {0xeb});
        }
    };

    void* get_prop(plug::PtrProp prop, void* data) override {
        switch (prop) {
        case plug::PtrProp::PState:
            return *reinterpret_cast<void**>(mem::get_base() + 0xb49d4);
        case plug::PtrProp::PNextFrame:
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x30);
        case plug::PtrProp::PNextData:
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x38);
        case plug::PtrProp::PSubTickStep:
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x4d8);
        case plug::PtrProp::PIsPaused:
            return reinterpret_cast<void*>(reinterpret_cast<size_t>(data) + 0x178);
        case plug::PtrProp::Update:
            return reinterpret_cast<void*>(mem::get_base() + 0x462e0);
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
        *buf = new PlugIwt;
    } else {
        check = mem::exe_name == "I WANNA TRY - A New Adventure Demo.exe";
    }
}

PLUG_REG(on_plugin_check)
