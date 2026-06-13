#define WIN32_LEAN_AND_MEAN
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include "../src/state.hpp"
#include "../src/winhooks.hpp"
#include "../tools/timer_fix.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ost::optional;
using ost::string_view;
using std::string;

class PlugIwbtb final : public plug::PlugBase {
private:
    void(__cdecl* SaveGameState)(void* hfile);
    void(__cdecl* LoadGameState)(void* hfile, unsigned int* outframe);

public:
    PlugIwbtb() {
        name = "I Wanna Be The Boshy";
        SaveGameState = nullptr;
        LoadGameState = nullptr;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 50;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x37dc0);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x39780);
        cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x365a0);
        cfg.pRenderFrame = reinterpret_cast<void*>(mem::get_base() + 0x1ebf0);
        cfg.pProcessTransition = reinterpret_cast<void*>(mem::get_base() + 0x1aac0);
        cfg.pRenderTransition = reinterpret_cast<void*>(mem::get_base() + 0x1cd40);
        mem::write(mem::get_base() + 0x4b74, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x4b6d, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x4c29, {0xeb});
        mem::write(mem::get_base() + 0x4659f, {0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x2a74, {0x90, 0x90});
        mem::write(mem::get_base() + 0x2994, {0x90, 0x90});
        mem::write(mem::get_base() + 0x299f, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x29ad, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x1d6d7,
                   {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}); // fixed switching the scene
        mem::write(mem::get_base() + 0x1d77e, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x2a49, {0xeb});
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

    bool update_init() override {
        auto& cfg = conf::get();
        cfg.tm_fix_event_entry_offset = 0x8;
        cfg.tm_fix_event_entry_type_offset = 0x9;
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

    std::pair<float, float> mouse_from_window(int x, int y) override {
        if (x < 0 || y < 0)
            return {-1.f, -1.f};
        auto win_size = winhooks::get_client_size();
        return {static_cast<float>(x) / static_cast<float>(win_size.first),
                static_cast<float>(y) / static_cast<float>(win_size.second)};
    }

    std::pair<int, int> mouse_to_window(float x, float y) override {
        if (x < 0.f || y < 0.f)
            return {-100, -100};
        auto win_size = winhooks::get_client_size();
        return {static_cast<int>(x * static_cast<float>(win_size.first)),
                static_cast<int>(y * static_cast<float>(win_size.second))};
    }

    void* get_prop(plug::PtrProp prop, void* data) override {
        switch (prop) {
        case plug::PtrProp::PState:
            return *reinterpret_cast<void**>(mem::get_base() + 0x59a9c);
        case plug::PtrProp::PStats:
            return *reinterpret_cast<void**>(mem::get_base() + 0x59a98);
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
            // Check if LoadGameState already failed
            if (!state::is_processing_save())
                return {};
            return timer_fix::load(timer_data);
        }
        return {};
    }
};

static void on_plugin_check_iwbtb(plug::PlugBase** buf, bool& check) {
    if (buf) {
        *buf = new PlugIwbtb;
    } else {
        check = mem::exe_name == "I Wanna Be The Boshy.exe";
    }
}

PLUG_REG(PlugIwbtb, on_plugin_check_iwbtb)
