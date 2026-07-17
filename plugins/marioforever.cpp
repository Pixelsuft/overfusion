#define WIN32_LEAN_AND_MEAN
#include "../src/config.hpp"
#include "../src/log.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include "../src/state.hpp"
#include "../src/winhooks.hpp"
#include "../tools/timer_fix.hpp"
#include <Windows.h>

using of::string_view;
using std::string;

class PlugMarioForever final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);
    size_t trans_addr;

public:
    PlugMarioForever() {
        name = "Mario Forever";
        cmdline_append = string(" /SF \"") + string(ofs::get_cwd()) + "\\data.exe\" /SO394240";
        SaveGameState = nullptr;
        LoadGameState = nullptr;
        trans_addr = 0;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 60;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x469f0);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x485e0);
        cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x449b0);
        cfg.pRenderFrame = reinterpret_cast<void*>(mem::get_base() + 0x2acc0);
        cfg.pProcessTransition = reinterpret_cast<void*>(mem::get_base() + 0x27380);
        cfg.pRenderTransition = reinterpret_cast<void*>(mem::get_base() + 0x28790);
        // No waiting
        mem::write(mem::get_base() + 0x2f57, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        mem::write(mem::get_base() + 0x2f28, {0xeb});
        // Game FPS is fine
        mem::write(mem::get_base() + 0x292ba, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // Game title
        mem::write(mem::get_base() + 0x25bcd, {0xeb});
        mem::write(mem::get_base() + 0x25bf8, {0x90, 0x90});
        return true;
    }

    bool update_init() override {
        auto& cfg = conf::get();
        // cfg.tm_fix_event_entry_offset = 0xe;
        // cfg.tm_fix_event_entry_type_offset = 0x10;
        return true;
    }

    of::optional<std::string> before_dll_load(string_view path, string_view fn) override {
        if (fn == "wininet.dll")
            return "";
        // of::info("Before load {}", fn);
        return {};
    }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        size_t base = reinterpret_cast<size_t>(mod);
        if (fn == "cctrans.dll") {
            trans_addr = base + 0x78b8;
        } else if (fn == "Onu.mfx") {
            if (conf::get().disable_audio)
                mem::write(base + 0x66a9, {0xeb});
        }
    };

    bool set_trans_enabled(bool enabled) override {
        if (trans_addr == 0)
            return false;
        return enabled ? mem::write<true>(trans_addr, {0x74})
                       : mem::write<true>(trans_addr, {0xeb});
    }

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
            return *reinterpret_cast<void**>(mem::get_base() + 0xab4bc);
        case plug::PtrProp::PStats:
            return *reinterpret_cast<void**>(mem::get_base() + 0xab4b8);
        case plug::PtrProp::PGlobalApp:
            return *reinterpret_cast<void**>(mem::get_base() + 0xab4b4);
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

    of::expected<void, string> save_state(ofs::File& file) override {
        if (conf::get().save_game_state)
            SaveGameState(file.get_handle());
        return {};
    }

    of::expected<void, string> load_state(ofs::File& file) override {
        unsigned int outframe = 0;
        if (!conf::get().is_replay)
            LoadGameState(file.get_handle(), &outframe);
        return {};
    }

    static of::optional<PlugMarioForever*> on_plugin_check() {
        // TODO: improve
        if (mem::exe_name == "stdrt.exe" && ofs::file_exists(string(ofs::get_cwd()) + "\\data.exe"))
            return new PlugMarioForever;
        return {};
    }
};

PLUG_REG(PlugMarioForever);
