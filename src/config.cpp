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

static conf::Task task_from_string(string_view sv) {
    static std::map<string_view, conf::Task> task_map = {
        {"save", conf::Task::SaveState},     {"load", conf::Task::LoadState},
        {"advance", conf::Task::Advance},    {"play", conf::Task::Play},
        {"fast", conf::Task::FastForward},   {"map", conf::Task::Map},
        {"push_temp", conf::Task::PushTemp}, {"menu", conf::Task::Menu}};
    auto it = task_map.find(sv);
    if (it == task_map.end()) {
        spdlog::warn("Unknown task: {}", sv);
        return conf::Task::None;
    }
    return it->second;
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
            bind.key = 0;
            if (val["key"].is_string()) {
                string skey = val["key"];
                std::transform(skey.begin(), skey.end(), skey.begin(),
                               [](int c) { return std::tolower(c); });
                bind.key = input::vk_from_string(skey).value_or(0);
                if (bind.key == 0)
                    continue;
            } else if (val["key"].is_number_integer()) {
                bind.key = val["key"];
                if (!is_valid_vk(bind.key)) {
                    spdlog::warn("Invalid bind key value: {}", bind.key);
                    continue;
                }
            } else {
                spdlog::warn("Invalid bind key type");
                continue;
            }
            bind.task = task_from_string(val["task"]);
            if (bind.task == Task::None)
                continue;
            if (val["mods"].is_array()) {
                for (auto& mod : val["mods"]) {
                    if (mod.is_string()) {
                        string skey = mod;
                        std::transform(skey.begin(), skey.end(), skey.begin(),
                                       [](int c) { return std::tolower(c); });
                        auto key = input::vk_from_string(skey).value_or(0);
                        if (key != 0)
                            bind.mods.push_back(key);
                    } else if (mod.is_number_integer()) {
                        int key = mod;
                        if (is_valid_vk(key))
                            bind.mods.push_back(key);
                        else
                            spdlog::warn("Invalid key mod value: {}", key);
                    } else
                        spdlog::warn("Invalid key mod value type");
                }
            }
            bind.extra = 0;
            if ((bind.task == Task::SaveState || bind.task == Task::LoadState) &&
                val["slot"].is_number_integer()) {
                bind.extra = val["slot"];
                ENSURE(bind.extra >= 0);
            } else if ((bind.task == Task::Play || bind.task == Task::FastForward) &&
                       val["toggle"].is_boolean())
                bind.extra = val["toggle"];
            else if (bind.task == Task::Map) {
                if (val["target"].is_string()) {
                    string skey = val["target"];
                    std::transform(skey.begin(), skey.end(), skey.begin(),
                                   [](int c) { return std::tolower(c); });
                    bind.extra = input::vk_from_string(skey).value_or(0);
                    if (bind.extra == 0)
                        continue;
                } else if (val["target"].is_number_integer()) {
                    bind.extra = val["target"];
                    if (!is_valid_vk(bind.extra)) {
                        spdlog::warn("Invalid bind map target {}", bind.extra);
                        continue;
                    }
                } else {
                    spdlog::warn("Skipped bind without target");
                    continue;
                }
                if (bind.extra == VK_LBUTTON || bind.extra == VK_MBUTTON ||
                    bind.extra == VK_RBUTTON) {
                    spdlog::warn("Cannot use create bind 'map' for mouse buttons");
                    continue;
                }
            } else if (bind.task == Task::PushTemp) {
                spdlog::debug("TODO: PushTemp task");
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
