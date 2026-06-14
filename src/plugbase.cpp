#include "plugbase.hpp"
#include "ass.hpp"
#include <spdlog/spdlog.h>
#include <vector>

using plug::PlugBase;

PlugBase::PlugBase() : name("Abstract plugin") {}

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

void PlugBase::execute_triggered_event(unsigned int p) {}

PlugBase::~PlugBase() {}

static PlugBase* _cur_plug;

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

PlugBase& plug::get() {
    ASS(_cur_plug != nullptr);
    return *_cur_plug;
}
