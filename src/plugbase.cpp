#include "plugbase.hpp"
#include "ass.hpp"
#include <spdlog/spdlog.h>
#include <vector>

static plug::PlugBase* _cur_plug;

static std::vector<plug::PlugCheckCallback>& get_registry() {
    static std::vector<plug::PlugCheckCallback> registry;
    return registry;
}

void plug::reg(plug::PlugCheckCallback callback) { get_registry().push_back(callback); }

bool plug::search_and_run() {
    PlugCheckCallback cb = nullptr;
    auto& reg = get_registry();
    bool check_bool = false;
    for (const auto& temp_cb : reg) {
        temp_cb(nullptr, check_bool);
        if (check_bool) {
            cb = temp_cb;
            break;
        }
    }
    reg.clear();
    if (cb == nullptr) {
        ENSURE(false);
        spdlog::error("Failed to find plugin for the game");
        return false;
    }
    cb(&_cur_plug, check_bool);
    if (!_cur_plug) {
        ENSURE(false);
        spdlog::error("Failed to init plugin for the game");
        return false;
    }
    spdlog::info("Plugin initialized: {}", _cur_plug->name);
    return true;
}

plug::PlugBase& plug::get() {
    ASS(_cur_plug != nullptr);
    return *_cur_plug;
}
