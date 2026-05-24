#define WIN32_LEAN_AND_MEAN
#include "config.hpp"
#include "ass.hpp"
#include "files.hpp"
#include "input.hpp"
#include "ofs.hpp"
#include <Windows.h>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

using conf::Config;
using ost::string_view;
using std::string;

static Config* _conf_ptr;

static nlohmann::json read_config_file(string& proj_name) {
    auto path = std::string(files::get_cwd()) + '\\' + proj_name + "\\overfusion.json";
    spdlog::info("Config path: {}", path);
    ofs::File file(path, 0);
    if (!file.is_open()) {
        spdlog::warn("Config file not found");
        return {};
    }
    auto size = file.size();
    if (size < 2 || size > (1024 * 1024 * 25)) {
        spdlog::error("Invalid config file size");
        return {};
    }
    char* buf = new char[static_cast<size_t>(size)];
    if (!file.read(buf, static_cast<size_t>(size))) {
        spdlog::error("Failed to read config file");
        delete[] buf;
        return {};
    }
    file.close();
#if defined(_HAS_EXCEPTIONS) && _HAS_EXCEPTIONS == 1
    try {
        nlohmann::json data = nlohmann::json::parse(buf, buf + size);
        delete[] buf;
        return data;
    } catch (const nlohmann::json::parse_error& e) {
        delete[] buf;
        spdlog::error("Failed to parse config: {}", e.what());
        return {};
    }
#else
    nlohmann::json data = nlohmann::json::parse(buf, buf + size, nullptr, false);
    delete[] buf;
    if (data.is_discarded()) {
        spdlog::error("Failed to parse config: invalid JSON syntax");
        return {};
    }
    return data;
#endif
}

constexpr bool is_valid_vk(int vk) { return vk > 0 && vk < 256; }

static ost::optional<conf::Task> task_from_string(string_view sv) {
    static std::map<string_view, conf::Task> task_map = {
        {"save", conf::Task::SaveState},     {"load", conf::Task::LoadState},
        {"advance", conf::Task::Advance},    {"play", conf::Task::Play},
        {"fast", conf::Task::FastForward},   {"map", conf::Task::Map},
        {"hold_temp", conf::Task::HoldTemp}, {"menu", conf::Task::Menu}};
    auto it = task_map.find(sv);
    if (it == task_map.end()) {
        // TODO: maybe move warns/errors from this funcs to top level funcs??
        spdlog::warn("Unknown task: {}", sv);
        return {};
    }
    return it->second;
}

static ost::optional<int> key_from_json(nlohmann::json& val) {
    if (val.is_string()) {
        return input::vk_from_string(val);
    } else if (val.is_number_integer()) {
        int ret = val;
        if (!is_valid_vk(ret)) {
            spdlog::warn("Invalid key value: {}", ret);
            return {};
        }
        return ret;
    } else {
        spdlog::warn("Invalid key value type: {}", val.type_name());
        return {};
    }
}

Config::Config() {
    project_name = "test_proj"; // TODO: configure it
    ffmpeg_cmdline =
        "ffmpeg -y -f:v rawvideo -s $SIZE -pix_fmt rgb32 -r $FPS -i - -an $PROJ/$NAME.mp4";
    system_offset = local_offset = startup_offset = 0; // TODO: conf them
    tm_fix_event_entry_offset = tm_fix_event_entry_type_offset = 0;
    pUpdateGameFrame = pRenderFrame = pProcessTransition = pRenderTransition = nullptr;
    fps = 0;
    show_menu = show_info = true;
    is_replay = false;
    is_paused = true;
    need_advance = false;
    fast_forward = false;
    emulate_user_timers = false;
    emulate_mm_timers = false;
    disable_threads = false;
    delay_thread_hook = false;
    is_unicode = false;
    custom_window = true;
    virtual_fs = true;
    no_ini_hooks = false;
    boxed_mode = false;
    reset_on_replay = false;
    save_game_state = true;
    disable_app_menu = false;
    allow_timers_fix = true;
    allow_d3d9_recording = true;
    allow_audio_hook = true;
    disable_audio = false;
    record_audio = false;
    support_audio_panning = true;
    allow_setting_cursor_pos = false;
}

#define READ_BOOL(name)                                                                            \
    if (data[#name].is_boolean() || data[#name].is_number_integer())                               \
    name = data[#name]

void Config::read() {
    auto temp_ret = ofs::make_dir(project_name);
    ENSURE(temp_ret);
    auto data = read_config_file(project_name);
    if (data["fps"].is_number_integer() && data["fps"].is_number_unsigned())
        fps = data["fps"];
    READ_BOOL(show_info);
    READ_BOOL(show_menu);
    READ_BOOL(emulate_user_timers);
    READ_BOOL(emulate_mm_timers);
    READ_BOOL(virtual_fs);
    READ_BOOL(disable_threads);
    READ_BOOL(delay_thread_hook);
    READ_BOOL(no_ini_hooks);
    READ_BOOL(reset_on_replay);
    READ_BOOL(save_game_state);
    READ_BOOL(disable_app_menu);
    READ_BOOL(allow_timers_fix);
    READ_BOOL(allow_d3d9_recording);
    READ_BOOL(allow_audio_hook);
    READ_BOOL(disable_audio);
    READ_BOOL(record_audio);
    READ_BOOL(support_audio_panning);
    READ_BOOL(allow_setting_cursor_pos);
    if (data["cmdline_append"].is_string())
        cmdline_append = data["cmdline_append"];
    if (data["ffmpeg_cmdline"].is_string())
        ffmpeg_cmdline = data["ffmpeg_cmdline"];
    if (data["binds"].is_array()) {
        for (auto& val : data["binds"]) {
            if (!val.is_object())
                continue;
            if (!val["task"].is_string()) {
                spdlog::warn("Invalid bind task setting");
                continue;
            }
            Bind bind;
            auto key_v = key_from_json(val["key"]);
            if (!key_v.has_value()) {
                spdlog::warn("Skipping bind (invalid key)");
                continue;
            }
            bind.key = key_v.value();
            auto temp_task = task_from_string(val["task"]);
            if (!temp_task.has_value())
                continue;
            bind.task = temp_task.value();
            if (val["mods"].is_array()) {
                for (auto& mod : val["mods"]) {
                    auto mod_v = key_from_json(mod);
                    if (mod_v.has_value())
                        bind.mods.push_back(mod_v.value());
                    else
                        spdlog::warn("Skipping mod key for bind");
                }
            }
            bind.extra = 0;
            switch (bind.task) {
            case Task::SaveState:
            case Task::LoadState:
                if (val["slot"].is_number_integer())
                    bind.extra = val["slot"];
                ENSURE(bind.extra >= 0);
                break;
            case Task::Play:
            case Task::FastForward:
                if (val["toggle"].is_boolean())
                    bind.extra = val["toggle"];
                break;
            case Task::Map: {
                auto target_v = key_from_json(val["target"]);
                if (!target_v.has_value()) {
                    spdlog::warn("Skipping 'map' bind with invalid target");
                    continue;
                }
                bind.extra = target_v.value();
                if (bind.extra == VK_LBUTTON || bind.extra == VK_MBUTTON ||
                    bind.extra == VK_RBUTTON) {
                    spdlog::warn("Cannot use create bind 'map' for mouse buttons");
                    continue;
                }
                break;
            }
            case Task::HoldTemp: {
                auto target_v = key_from_json(val["target"]);
                if (!target_v.has_value()) {
                    spdlog::warn("Skipping 'hold_temp' bind with invalid target");
                    continue;
                }
                bind.extra = target_v.value();
                if (bind.extra != VK_LBUTTON && bind.extra != VK_MBUTTON &&
                    bind.extra != VK_RBUTTON) {
                    spdlog::warn("Cannot use create bind 'hold_temp' for keyboard keys");
                    continue;
                }
                break;
            }
            case Task::Menu:
            case Task::Advance:
                break;
#ifndef _DEBUG
            default:
                ASS(false);
                break;
#endif
            }
            auto key_str = input::vk_to_string(bind.key);
            ASS(key_str.has_value());
            spdlog::info("Bind (task={}, extra={}, key='{}', mods={})", static_cast<int>(bind.task),
                         bind.extra, key_str.value(), bind.mods);
            binds.push_back(bind);
        }
        std::sort(binds.begin(), binds.end(),
                  [](const auto& a, const auto& b) { return a.key < b.key; });
    }
}

void conf::init() { _conf_ptr = new Config; }

Config& conf::get() {
    ASS(_conf_ptr != nullptr);
    return *_conf_ptr;
}
