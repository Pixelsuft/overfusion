#pragma once
#include "ofs.hpp"
#include <string>
#include <string_view>

#define PLUG_REG(check_cb)                                                                         \
    class Startup {                                                                                \
    public:                                                                                        \
        Startup() { plug::reg(check_cb); }                                                         \
    };                                                                                             \
    static Startup startup;

namespace plug {
enum class PtrProp { Update };

class PlugBase {
public:
    std::string name;
    int fps;
    bool unicode;

    PlugBase() {}
    virtual void pre_init() {}
    virtual void update_init() {}
    virtual void after_dll_load(std::string_view path, std::string_view fn, void* mod) {}
    virtual void* get_ptr_prop(PtrProp prop) = 0;
    virtual bool save_state(ofs::File& file) = 0;
    virtual bool load_state(ofs::File& file) = 0;
    virtual ~PlugBase() {}
};

using PlugCheckCallback = void (*)(PlugBase** buf, bool& check);
extern plug::PlugBase* _cur_plug;

bool search_and_run();
void reg(plug::PlugCheckCallback callback);
inline PlugBase& get() {
    __assume(_cur_plug != nullptr);
    return *_cur_plug;
}
} // namespace plug
