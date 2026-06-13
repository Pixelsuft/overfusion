#define WIN32_LEAN_AND_MEAN
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

class PlugFnaf3 final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);
    size_t trans_addr;

public:
    PlugFnaf3() {
        name = "Five Nights at Freddy's 3";
        SaveGameState = nullptr;
        LoadGameState = nullptr;
        trans_addr = 0;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 60;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x48080);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x49c70);
        cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x46010);
        cfg.pRenderFrame = reinterpret_cast<void*>(mem::get_base() + 0x2c270);
        cfg.pProcessTransition = reinterpret_cast<void*>(mem::get_base() + 0x28a80);
        cfg.pRenderTransition = reinterpret_cast<void*>(mem::get_base() + 0x29e10);
        // No waiting
        mem::write(mem::get_base() + 0x2fae, {0xeb});
        mem::write(mem::get_base() + 0x2fdb, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // By saying pause I mean pause
        mem::write(mem::get_base() + 0x2a9c8, {0xeb});
        // Game FPS is fine
        mem::write(mem::get_base() + 0x2aa40, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // Game title
        mem::write(mem::get_base() + 0x272ad, {0xeb});
        mem::write(mem::get_base() + 0x272d8, {0x90, 0x90});
        return true;
    }

    bool update_init() override {
        auto& cfg = conf::get();
        cfg.tm_fix_event_entry_offset = 0x8;
        cfg.tm_fix_event_entry_type_offset = 0x9;
        return true;
    }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        size_t base = reinterpret_cast<size_t>(mod);
        if (fn == "cctrans.dll") {
            trans_addr = base + 0x7557;
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
            return *reinterpret_cast<void**>(mem::get_base() + 0xb39d4);
        case plug::PtrProp::PStats:
            return *reinterpret_cast<void**>(mem::get_base() + 0xb39d0);
        case plug::PtrProp::PGlobalApp:
            return *reinterpret_cast<void**>(mem::get_base() + 0xb39cc);
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

static void on_plugin_check_fnaf3(plug::PlugBase** buf, bool& check) {
    if (buf) {
        *buf = new PlugFnaf3;
    } else {
        check = mem::exe_name == "FiveNightsatFreddys3.exe";
    }
}

PLUG_REG(PlugFnaf3, on_plugin_check_fnaf3)
