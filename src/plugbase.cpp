#include "plugbase.hpp"
#include "ass.hpp"
#include <spdlog/spdlog.h>
#include <vector>

using plug::PlugBase;

#ifndef JUMBO_BUILD
static std::vector<plug::PlugCheckCallback>& get_registry() {
    static std::vector<plug::PlugCheckCallback> registry;
    return registry;
}

void plug::_reg_internal(plug::PlugCheckCallback callback) { get_registry().push_back(callback); }
#endif

PlugBase::PlugBase() {}

bool PlugBase::pre_init() { return true; }

bool PlugBase::update_init() { return true; }

ost::optional<std::string> PlugBase::before_dll_load(ost::string_view path, ost::string_view fn) {
    return {};
}

void PlugBase::after_dll_load(ost::string_view path, ost::string_view fn, void* mod) {}

void* PlugBase::after_proc_get(void* module, const char* proc, void* ret) { return ret; }

bool PlugBase::set_trans_enabled(bool enabled) { return false; }

std::pair<float, float> PlugBase::mouse_from_window(int x, int y) { return {-1.f, -1.f}; }

std::pair<int, int> PlugBase::mouse_to_window(float x, float y) { return {-100, -100}; }

void PlugBase::draw_menu() {}

PlugBase::~PlugBase() {}

static PlugBase* _cur_plug;

bool plug::search_and_run() {
    _cur_plug = nullptr;
#ifdef JUMBO_BUILD
    if (false) {
    }
    JUMBO_PLUGIN_DETECTION()
    else {
        spdlog::error("Failed to find plugin for the game");
        return false;
    }
    if (!_cur_plug) {
        spdlog::error("Failed to spawn plugin for the game");
        return false;
    }
#else
    auto& reg = get_registry();
    for (const auto& temp_cb : reg) {
        if (auto val = temp_cb()) {
            _cur_plug = val.value();
            if (!_cur_plug) {
                spdlog::error("Failed to spawn plugin for the game");
                return false;
            }
            break;
        }
    }
#endif
    if (!_cur_plug) {
        spdlog::error("Failed to find plugin for the game");
        return false;
    }
    spdlog::info("Plugin initialized: {}", _cur_plug->name);
    return true;
}

PlugBase& plug::get() {
    ASS(_cur_plug != nullptr);
    return *_cur_plug;
}
