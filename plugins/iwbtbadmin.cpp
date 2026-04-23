#define WIN32_LEAN_AND_MEAN
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include "../src/state.hpp"
#include "../tools/timer_fix.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ost::optional;
using ost::string_view;
using std::string;

class PlugIwbtbAdmin final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);

public:
    PlugIwbtbAdmin() {
        name = "I Wanna Be The Boshy [admin]";
        need_key_message = false;
        SaveGameState = nullptr;
        LoadGameState = nullptr;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 50;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x4f9b0);
        ASS(SaveGameState != nullptr);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x51610);
        ASS(LoadGameState != nullptr);
        cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x4d3e0);
        cfg.pRenderFrame = reinterpret_cast<void*>(mem::get_base() + 0x31150);
        cfg.pProcessTransition = reinterpret_cast<void*>(mem::get_base() + 0x2c860);
        cfg.pRenderTransition = reinterpret_cast<void*>(mem::get_base() + 0x2e0a0);
        // No input polling
        mem::write(mem::get_base() + 0x2d077, {0xeb});
        mem::write(mem::get_base() + 0x2d969, {0xeb});
        // No waiting
        mem::write(mem::get_base() + 0x2fbb, {0xeb});
        mem::write(mem::get_base() + 0x302e, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x2ef41, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // Force input to use GetKeyState for immediate refresh
        mem::write(mem::get_base() + 0xb912, {0xeb});
        mem::write(mem::get_base() + 0xb9c2, {0xeb});
        // Title
        mem::write(mem::get_base() + 0x2cae6, {0xeb});
        mem::write(mem::get_base() + 0x2cb14, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x2cb1f, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x2cb29, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        return true;
    }

    bool update_init() override {
        auto& cfg = conf::get();
        cfg.gStats = *reinterpret_cast<void**>(mem::get_base() + 0xb60e8);
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
            return *reinterpret_cast<void**>(mem::get_base() + 0xb60ec);
        case plug::PtrProp::PGlobalApp:
            return *reinterpret_cast<void**>(mem::get_base() + 0xb60e4);
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
        if (conf::get().save_game_state) {
            std::vector<int> timer_data;
            auto timer_ret = timer_fix::save(timer_data);
            if (!timer_ret.has_value())
                return timer_ret;
            state::write_bin(file, timer_data);
            SaveGameState(file.get_handle());
        }
        return {};
    }

    ost::expected<void, string> load_state(ofs::File& file) override {
        unsigned int outframe = 0;
        if (!conf::get().is_replay) {
            std::vector<int> timer_data;
            state::load_bin(file, timer_data);
            LoadGameState(file.get_handle(), &outframe);
            if (!state::is_processing_save())
                return {};
            return timer_fix::load(timer_data);
        }
        return {};
    }
};

static void on_plugin_check(plug::PlugBase** buf, bool& check) {
    if (buf) {
        *buf = new PlugIwbtbAdmin;
    } else {
        check = mem::exe_name == "I Wanna Be The Boshy [admin].exe";
    }
}

PLUG_REG(PlugIwbtbAdmin, on_plugin_check)
