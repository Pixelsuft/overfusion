#define WIN32_LEAN_AND_MEAN
#include "../src/config.hpp"
#include "../src/mem.hpp"
#include "../src/plugbase.hpp"
#include "../src/state.hpp"
#include "../tools/perspective.hpp"
#include <Windows.h>

using of::string_view;
using std::string;

class PlugFnaf final : public plug::PlugBase {
private:
    void(__fastcall* SaveGameState)(void* hfile);
    void(__fastcall* LoadGameState)(void* hfile, unsigned int* outframe);
    inline static unsigned int(__stdcall* RandomO)(int dummy, unsigned short maxv);
    size_t trans_ptr;

public:
    PlugFnaf() {
        name = "Five Nights at Freddy's";
        SaveGameState = nullptr;
        LoadGameState = nullptr;
        RandomO = nullptr;
        trans_ptr = 0;
    }

    bool pre_init() override {
        auto& cfg = conf::get();
        if (cfg.fps <= 0)
            cfg.fps = 60;
        SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x47470);
        LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x49060);
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

    static unsigned int __stdcall RandomH(int dummy, unsigned short maxv) {
        auto ret = RandomO(dummy, maxv);
        return static_cast<unsigned int>(
            state::fetch_random_number(static_cast<int>(maxv), static_cast<int>(ret)));
    }

    bool update_init() override {
        // FIXME: random func got inlined in many parts. how to fix???
        // TODO: hook other important funcs
        hook::hook(mem::get_base() + 0x2c500, RandomH, &RandomO);
        return true;
    }

    void after_dll_load(string_view path, string_view fn, void* mod) override {
        if (mod == nullptr)
            return;
        size_t base = reinterpret_cast<size_t>(mod);
        if (fn == "mmfs2.dll") {
        } else if (fn == "cctrans.dll") {
            trans_ptr = base + 0x78b8;
        }
        perspective::after_dll_load(fn, mod);
    };

    void* after_proc_get(void* module, const char* proc, void* ret) override {
        return perspective::after_proc_get(module, proc, ret);
    }

    void draw_menu() override { perspective::draw_menu(); }

    bool set_trans_enabled(bool enabled) override {
        if (trans_ptr == 0)
            return false;
        return enabled ? mem::write<true>(trans_ptr, {0x74}) : mem::write<true>(trans_ptr, {0xeb});
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

    static of::optional<PlugFnaf*> on_plugin_check() {
        if (mem::exe_name == "FiveNightsatFreddys.exe")
            return new PlugFnaf;
        return {};
    }
};

PLUG_REG(PlugFnaf);
