#pragma once
#include "expect.hpp"
#include "ofs.hpp"
#include "opt.hpp"
#include "sv.hpp"
#include <string>

#define PLUG_REG(plug_class, check_cb)                                                             \
    class Startup_##plug_class {                                                                   \
    public:                                                                                        \
        Startup_##plug_class() { plug::reg(check_cb); }                                            \
    };                                                                                             \
    static Startup_##plug_class startup_##plug_class

namespace plug {
enum class PtrProp {
    PState,
    PGlobalApp,
    PStats,
    PSubTickStep,
    PIsPaused,
    PSceneID,
    PNextFrameTask,
    PNextFrameData,
    PSceneName
};

class PlugBase {
public:
    std::string name;
    std::string cmdline_append;

    PlugBase();
    // Early init
    virtual bool pre_init();
    // Init before processing first frame
    virtual bool update_init();
    // DLL load hook: nullopt for default value, empty string for failing
    virtual ost::optional<std::string> before_dll_load(ost::string_view path, ost::string_view fn);
    // When DLL was loaded
    virtual void after_dll_load(ost::string_view path, ost::string_view fn, void* mod);
    // GetProcAddress hook: return ret by default or your custom pointer
    virtual void* after_proc_get(void* module, const char* proc, void* ret);
    // You should implement transition enable/disable if your game uses so
    virtual bool set_trans_enabled(bool enabled);
    // Normalize mouse coordinates from window
    virtual std::pair<float, float> mouse_from_window(int x, int y);
    // Get window coordinates from normalized
    virtual std::pair<int, int> mouse_to_window(float x, float y);
    // Draw menu inside the "Plugin" collapsing header
    virtual void draw_menu();
    // Get pointer to something
    virtual void* get_prop(PtrProp prop, void* data = nullptr) = 0;
    // Save game and your data here
    virtual ost::expected<void, std::string> save_state(ofs::File& file) = 0;
    // Load game and your data here
    virtual ost::expected<void, std::string> load_state(ofs::File& file) = 0;
    virtual ~PlugBase();
};

using PlugCheckCallback = void (*)(PlugBase** buf, bool& check);

bool search_and_run();
void reg(plug::PlugCheckCallback callback);
PlugBase& get();
} // namespace plug
