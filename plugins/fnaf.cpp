#define WIN32_LEAN_AND_MEAN
#include "../src/ass.hpp"
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include "../src/winhooks.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ost::optional;
using ost::string_view;
using std::string;

class PlugFnaf final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);
    size_t trans_ptr;

public:
    PlugFnaf() {
        name = "Five Nights at Freddy's";
        need_key_message = false;
        SaveGameState = nullptr;
        LoadGameState = nullptr;
        trans_ptr = 0;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 60;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x47470);
        ASS(SaveGameState != nullptr);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x49060);
        ASS(LoadGameState != nullptr);
        cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x45430);
        cfg.pRenderFrame = reinterpret_cast<void*>(mem::get_base() + 0x2b970);
        cfg.pProcessTransition = reinterpret_cast<void*>(mem::get_base() + 0x28060);
        cfg.pRenderTransition = reinterpret_cast<void*>(mem::get_base() + 0x29470);
        // No waiting
        // mem::write(mem::get_base() + 0x2ea5, {0xeb});
        mem::write(mem::get_base() + 0x2f28, {0xeb});
        mem::write(mem::get_base() + 0x2f57, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // Use high precision timer instead of ugly SetTimer
        mem::write(mem::get_base() + 0x23b88, {0xeb});
        // By saying pause I mean pause
        mem::write(mem::get_base() + 0x5784b, {0xeb});
        mem::write(mem::get_base() + 0x57891, {0xeb});
        // Game FPS is fine
        mem::write(mem::get_base() + 0x29f7a, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // Game title
        mem::write(mem::get_base() + 0x268d8, {0x90, 0x90});
        mem::write(mem::get_base() + 0x268df, {0x90, 0x90});
        mem::write(mem::get_base() + 0x268e5, {0x90, 0x90});
        return true;
    }

    bool update_init() override { return true; }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        size_t base = reinterpret_cast<size_t>(mod);
        if (fn == "mmfs2.dll") {
        } else if (fn == "cctrans.dll") {
            trans_ptr = base + 0x78b8;
            // Disable transitions
            // mem::write(base + 0x78b8, {0xeb});
        } else if (fn == "Perspective.mfx") {
            // Patch for disabling Perspective
            // mem::write(base + 0x169d, {0x31, 0xc0, 0xc2, 0x04, 0x00});
        }
    };

    std::pair<float, float> mouse_from_screen(int x, int y) override {
        if (x < 0 || y < 0)
            return {-1.f, -1.f};
        auto win_size = winhooks::get_size();
        return {static_cast<float>(x) / static_cast<float>(win_size.first),
                static_cast<float>(y) / static_cast<float>(win_size.second)};
    }

    std::pair<int, int> mouse_to_screen(float x, float y) override {
        if (x < 0.f || y < 0.f)
            return {-100, -100};
        auto win_size = winhooks::get_size();
        return {static_cast<int>(x * static_cast<float>(win_size.first)),
                static_cast<int>(y * static_cast<float>(win_size.second))};
    }

    void* get_prop(plug::PtrProp prop, void* data) override {
        switch (prop) {
        case plug::PtrProp::PState:
            return *reinterpret_cast<void**>(mem::get_base() + 0xac9b4);
        case plug::PtrProp::PStats:
            return *reinterpret_cast<void**>(mem::get_base() + 0xac9b0);
        case plug::PtrProp::PGlobalApp:
            return *reinterpret_cast<void**>(mem::get_base() + 0xac9ac);
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
        if (conf::get().save_game_state)
            SaveGameState(file.get_handle());
        return {};
    }

    ost::expected<void, string> load_state(ofs::File& file) override {
        unsigned int outframe = 0;
        if (!conf::get().is_replay)
            LoadGameState(file.get_handle(), &outframe);
        return {};
    }
};

static void on_plugin_check_fnaf(plug::PlugBase** buf, bool& check) {
    if (buf) {
        *buf = new PlugFnaf;
    } else {
        check = mem::exe_name == "FiveNightsatFreddys.exe";
    }
}

PLUG_REG(PlugFnaf, on_plugin_check_fnaf)
