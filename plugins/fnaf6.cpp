#define WIN32_LEAN_AND_MEAN
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include "../src/state.hpp"
#include "../tools/perspective.hpp"
#include "../tools/timer_fix.hpp"
#include <Windows.h>

class PlugFnaf6 final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);
    size_t trans_addr;

public:
    PlugFnaf6() {
        name = std::string("Freddy Fazbear's Pizzeria Simulator");
        SaveGameState = nullptr;
        LoadGameState = nullptr;
        trans_addr = 0;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 60;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x480d0);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x49cc0);
        cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x46060);
        cfg.pRenderFrame = reinterpret_cast<void*>(mem::get_base() + 0x2c300);
        cfg.pProcessTransition = reinterpret_cast<void*>(mem::get_base() + 0x28ae0);
        cfg.pRenderTransition = reinterpret_cast<void*>(mem::get_base() + 0x29ea0);
        // No waiting
        mem::write(mem::get_base() + 0x2fae, {0xeb});
        mem::write(mem::get_base() + 0x2fdb, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // By saying pause I mean pause
        mem::write(mem::get_base() + 0x2aa58, {0xeb});
        // Game FPS is fine
        mem::write(mem::get_base() + 0x2aad0, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
        // Game title
        mem::write(mem::get_base() + 0x272fd, {0xeb});
        mem::write(mem::get_base() + 0x27328, {0x90, 0x90});
        return true;
    }

    bool update_init() override {
        auto& cfg = conf::get();
        cfg.tm_fix_event_entry_offset = 0x10;
        cfg.tm_fix_event_entry_type_offset = 0x12;
        return true;
    }

    void after_dll_load(of::string_view path, of::string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        size_t base = reinterpret_cast<size_t>(mod);
        if (fn == "cctrans.dll") {
            trans_addr = base + 0x7557;
        }
        perspective::after_dll_load(fn, mod);
    };

    void* after_proc_get(void* module, const char* proc, void* ret) override {
        return perspective::after_proc_get(module, proc, ret);
    }

    void draw_menu() override { perspective::draw_menu(); }

    bool set_trans_enabled(bool enabled) override {
        if (trans_addr == 0)
            return false;
        return enabled ? mem::write<true>(trans_addr, {0x74})
                       : mem::write<true>(trans_addr, {0xeb});
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

    of::expected<void, std::string> save_state(ofs::File& file) override {
        if (conf::get().save_game_state) {
            std::vector<IntPair> timer_data;
            auto timer_ret = timer_fix::save(timer_data);
            if (!timer_ret.has_value())
                return timer_ret;
            state::write_bin(file, timer_data);
            SaveGameState(file.get_handle());
        }
        return {};
    }

    of::expected<void, std::string> load_state(ofs::File& file) override {
        unsigned int outframe = 0;
        if (!conf::get().is_replay) {
            std::vector<IntPair> timer_data;
            state::load_bin(file, timer_data);
            LoadGameState(file.get_handle(), &outframe);
            if (!conf::get().processing_save)
                return {};
            return timer_fix::load(std::move(timer_data));
        }
        return {};
    }

    static of::optional<PlugFnaf6*> on_plugin_check() {
        if (mem::exe_name == "Pizzeria Simulator.exe")
            return new PlugFnaf6;
        return {};
    }
};

PLUG_REG(PlugFnaf6);
