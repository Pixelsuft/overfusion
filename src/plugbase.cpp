#include "plugbase.hpp"
#include "ass.hpp"
#include "log.hpp"
#include "winhooks.hpp"
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

of::optional<std::string> PlugBase::before_dll_load(of::string_view path, of::string_view fn) {
    return {};
}

void PlugBase::after_dll_load(of::string_view path, of::string_view fn, void* mod) {}

void* PlugBase::after_proc_get(void* module, const char* proc, void* ret) { return ret; }

void PlugBase::early_update() {}

bool PlugBase::set_trans_enabled(bool enabled) { return false; }

std::pair<float, float> PlugBase::mouse_from_window(int x, int y) {
    if (x < 0 || y < 0)
        return {-1.f, -1.f};
    auto win_size = winhooks::get_client_size();
    return {static_cast<float>(x) / static_cast<float>(win_size.first),
            static_cast<float>(y) / static_cast<float>(win_size.second)};
}

std::pair<int, int> PlugBase::mouse_to_window(float x, float y) {
    if (x < 0.f || y < 0.f)
        return {-100, -100};
    auto win_size = winhooks::get_client_size();
    return {static_cast<int>(x * static_cast<float>(win_size.first)),
            static_cast<int>(y * static_cast<float>(win_size.second))};
}

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
        of::error("Failed to find plugin for the game");
        return false;
    }
    if (!_cur_plug) {
        of::error("Failed to spawn plugin for the game");
        return false;
    }
#else
    auto& reg = get_registry();
    for (const auto& temp_cb : reg) {
        if (auto val = temp_cb()) {
            _cur_plug = val.value();
            if (!_cur_plug) {
                of::error("Failed to spawn plugin for the game");
                return false;
            }
            break;
        }
    }
#endif
    if (!_cur_plug) {
        of::error("Failed to find plugin for the game");
        return false;
    }
    of::info("Plugin initialized: {}", _cur_plug->name);
    return true;
}

PlugBase& plug::get() {
    ASS(_cur_plug != nullptr);
    return *_cur_plug;
}
