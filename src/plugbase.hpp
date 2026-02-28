#pragma once
#include "ofs.hpp"
#include "sv.hpp"
#include <optional>
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
    PSubTickStep,
    PIsPaused,
    PSceneID,
    PNextFrameTask,
    PNextFrameData,
    PSceneName,
    Update,
    Render
};
enum class BoolProp { NeedKeyMsg };

class PlugBase {
public:
    std::string name;
    bool unicode;

    PlugBase() : name("Abstract plugin"), unicode(false) {}
    virtual bool pre_init() { return true; }
    virtual bool update_init() { return true; }
    virtual std::optional<std::string> before_dll_load(ost::string_view path, ost::string_view fn) {
        return {};
    }
    virtual void after_dll_load(ost::string_view path, ost::string_view fn, void* mod) {}
    virtual void* after_proc_get(void* module, const char* proc, void* ret) { return ret; }
    virtual void* get_prop(PtrProp prop, void* data = nullptr) = 0;
    virtual bool get_bool_prop(BoolProp prop) = 0;
    virtual bool save_state(ofs::File& file) = 0;
    virtual bool load_state(ofs::File& file) = 0;
    virtual ~PlugBase() {}
};

using PlugCheckCallback = void (*)(PlugBase** buf, bool& check);

bool search_and_run();
void reg(plug::PlugCheckCallback callback);
PlugBase& get();
} // namespace plug
