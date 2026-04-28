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
    static Startup_##plug_class startup_##plug_class;

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
    bool need_key_message;

    PlugBase() : name("Abstract plugin"), need_key_message(false) {}
    virtual bool pre_init() { return true; }
    virtual bool update_init() { return true; }
    virtual ost::optional<std::string> before_dll_load(ost::string_view path, ost::string_view fn) {
        return {};
    }
    virtual void after_dll_load(ost::string_view path, ost::string_view fn, void* mod) {}
    virtual void* after_proc_get(void* module, const char* proc, void* ret) { return ret; }
    virtual std::pair<float, float> mouse_from_screen(int x, int y) { return {0.f, 0.f}; }
    virtual std::pair<int, int> mouse_to_screen(float x, float y) { return {0, 0}; }
    virtual void* get_prop(PtrProp prop, void* data = nullptr) = 0;
    virtual ost::expected<void, std::string> save_state(ofs::File& file) = 0;
    virtual ost::expected<void, std::string> load_state(ofs::File& file) = 0;
    virtual ~PlugBase() {}
};

using PlugCheckCallback = void (*)(PlugBase** buf, bool& check);

bool search_and_run();
void reg(plug::PlugCheckCallback callback);
PlugBase& get();
} // namespace plug
