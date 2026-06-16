#define WIN32_LEAN_AND_MEAN
#include "config.hpp"
#include "ass.hpp"
#include "files.hpp"
#include "input.hpp"
#include "ofs.hpp"
#include "process.hpp"
#include <Windows.h>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>
#undef max
#undef min

using conf::Config;
using ost::string_view;
using std::string;

static Config* _conf_ptr;

static nlohmann::json read_config_file(ost::string_view path) {
    // Try to read and parse config file from disk
    spdlog::info("Reading config: {}", path);
    ofs::File file(path, 0);
    if (!file.is_open()) {
        spdlog::warn("Config file not found");
        return {};
    }
    auto size = file.size();
    if (size < 2 || size > (1024 * 1024 * 16)) {
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
    static const std::map<string_view, conf::Task> task_map = {
        {"save", conf::Task::SaveState},
        {"load", conf::Task::LoadState},
        {"advance", conf::Task::Advance},
        {"play", conf::Task::Play},
        {"fast", conf::Task::FastForward},
        {"map", conf::Task::Map},
        {"mouse_down", conf::Task::MouseDown},
        {"mouse_move", conf::Task::MouseMove},
        {"menu", conf::Task::Menu},
        {"replay_mode", conf::Task::ReplayMode},
        {"reset_game", conf::Task::ResetGame}};
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
    auto env_name = process::get_env("OVERFUSION_PROJECT_NAME");
    if (env_name.has_value()) {
        project_name = std::move(env_name.value());
        spdlog::info("Project name: {}", project_name);
    } else {
        spdlog::warn("No project name was specified, defaulting to 'test_proj'");
        project_name = "test_proj";
    }
    // TODO: validate project_name
    ffmpeg_cmdline =
        "ffmpeg -y -f:v rawvideo -s $SIZE -pix_fmt rgb32 -r $FPS -i - -an $PROJ/$NAME.mp4";
    system_offset = local_offset = startup_offset = 0;
    tm_fix_event_entry_offset = tm_fix_event_entry_type_offset = 0;
    pUpdateGameFrame = pRenderFrame = pProcessTransition = pRenderTransition = nullptr;
    forced_res = {0, 0};
    speed = 1.f;
    font_scale = 1.f;
    fps = 0;
    delta_multiplier = 1;
    render_type = RenderType::None;
    show_menu = show_info = true;
    is_replay = false;
    is_paused = true;
    need_advance = false;
    fast_forward = false;
    emulate_user_timers = false;
    emulate_mm_timers = false;
    disable_threads = false;
    delay_thread_hook = false;
    custom_window = true;
    virtual_fs = true;
    no_ini_hooks = false;
    boxed_mode = false;
    reset_on_replay = false;
    save_game_state = true;
    disable_app_menu = false;
    allow_timers_fix = true;
    allow_direct_capture = true;
    allow_audio_hook = true;
    disable_audio = false;
    record_audio = false;
    support_audio_panning = true;
    need_key_message = false;
    need_mouse_move_message = false;
    no_mouse_manipulation = false;
    draw_cursor = false;
    pixel_filter = false;
    ui_pixel_filter = false;
    save_vfs = false;
    disable_dark_mode_support = false;
    force_custom_window = false;
    wait_for_debugger = false;
    disable_fullscreen = false;
    disable_perspective = false;
    disable_viewport = false;
    pause_on_scene_switch = false;
}

// Read boolean variable
#define READ_BOOL(name)                                                                            \
    if (data[#name].is_boolean() || data[#name].is_number_integer())                               \
    name = data[#name]

void Config::read() {
    auto temp_ret = ofs::make_dir(project_name);
    ENSURE(temp_ret);
    auto data = read_config_file(string(files::get_cwd()) + "\\overfusion.json");
    auto proj_data =
        read_config_file(string(files::get_cwd()) + '\\' + project_name + "\\ofproject.json");
    if (proj_data["fps"].is_number_unsigned())
        fps = proj_data["fps"];
    if (proj_data["delta_multiplier"].is_number_unsigned())
        delta_multiplier = proj_data["delta_multiplier"];
    auto& fr = data["force_resolution"];
    if (fr.is_array() && fr.size() > 1) {
        if (fr[0].is_number_unsigned())
            forced_res.first = fr[0];
        if (fr[1].is_number_unsigned())
            forced_res.second = fr[1];
    }
    if (data["speed"].is_number_float()) {
        speed = data["speed"];
        speed = std::min(std::max(speed, 0.05f), 2.f);
    }
    if (data["font_scale"].is_number_float()) {
        font_scale = data["font_scale"];
        font_scale = std::min(std::max(font_scale, 0.05f), 3.f);
    }
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
    READ_BOOL(allow_direct_capture);
    READ_BOOL(allow_audio_hook);
    READ_BOOL(disable_audio);
    READ_BOOL(record_audio);
    READ_BOOL(support_audio_panning);
    READ_BOOL(draw_cursor);
    READ_BOOL(pixel_filter);
    READ_BOOL(ui_pixel_filter);
    READ_BOOL(save_vfs);
    READ_BOOL(disable_dark_mode_support);
    READ_BOOL(force_custom_window);
    READ_BOOL(wait_for_debugger);
    READ_BOOL(disable_fullscreen);
    READ_BOOL(disable_perspective);
    READ_BOOL(disable_viewport);
    READ_BOOL(pause_on_scene_switch);
    if (proj_data["system_time_offset"].is_number_unsigned())
        system_offset = proj_data["system_time_offset"];
    if (proj_data["local_time_offset"].is_number_unsigned())
        local_offset = proj_data["local_time_offset"];
    if (proj_data["startup_time_offset"].is_number_unsigned())
        startup_offset = proj_data["startup_time_offset"];
    if (proj_data["no_mouse_manipulation"].is_boolean() ||
        proj_data["no_mouse_manipulation"].is_number_integer())
        no_mouse_manipulation = proj_data["no_mouse_manipulation"];
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
                bind.extra = std::max(bind.extra, 0);
                break;
            case Task::Play:
            case Task::FastForward:
                if (val["toggle"].is_boolean())
                    bind.extra = val["toggle"];
                break;
            case Task::ReplayMode:
                if (val["value"].is_number_integer()) {
                    bind.extra = val["value"];
                    bind.extra = std::min(std::max(bind.extra, -1), 1);
                }
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
            case Task::MouseDown: {
                auto target_v = key_from_json(val["target"]);
                if (!target_v.has_value()) {
                    spdlog::warn("Skipping 'mouse_down' bind with invalid target");
                    continue;
                }
                bind.extra = target_v.value();
                if (bind.extra != VK_LBUTTON && bind.extra != VK_MBUTTON &&
                    bind.extra != VK_RBUTTON) {
                    spdlog::warn("Cannot use create bind 'mouse_down' for keyboard keys");
                    continue;
                }
                break;
            }
            case Task::ResetGame:
            case Task::MouseMove:
            case Task::Menu:
            case Task::Advance:
                break;
#ifndef _DEBUG
            default:
                ASS(false);
                break;
#endif
            }
            // Get string keys from ints
            auto key_str = input::vk_to_string(bind.key);
            ASS(key_str.has_value());
            std::vector<string> mods_str;
            for (int mod : bind.mods) {
                auto v = input::vk_to_string(mod);
                ASS(v.has_value());
                mods_str.push_back(string(v.value()));
            }
            spdlog::info("Bind (task={}, extra={}, key='{}', mods={})", static_cast<int>(bind.task),
                         bind.extra, key_str.value(), mods_str);
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
