#define WIN32_LEAN_AND_MEAN
#include "config.hpp"
#include "filehooks.hpp"
#include "ofs.hpp"
#include <Windows.h>
#include <algorithm>
#include <map>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

using conf::Config, std::string, ost::string_view;

static Config* _conf_ptr;

static nlohmann::json read_config_file() {
    auto path = std::string(filehooks::get_cwd()) + "\\overfusion.json";
    spdlog::debug("Config path: {}", path);
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
    try {
        nlohmann::json data = nlohmann::json::parse(buf, buf + size);
        delete[] buf;
        return data;
    } catch (const nlohmann::json::parse_error& e) {
        delete[] buf;
        spdlog::error("Failed to parse config: {}", e.what());
        return {};
    }
}

static int vk_from_string(string_view sv) {
    static std::map<std::string_view, int> vk_map = {
        {"f1", VK_F1},
        {"f2", VK_F2},
        {"f3", VK_F3},
        {"f4", VK_F4},
        {"f5", VK_F5},
        {"f6", VK_F6},
        {"f7", VK_F7},
        {"f8", VK_F8},
        {"f9", VK_F9},
        {"f10", VK_F10},
        {"f11", VK_F11},
        {"f12", VK_F12},
        {"f13", VK_F13},
        {"f14", VK_F14},
        {"f15", VK_F15},
        {"f16", VK_F16},
        {"f17", VK_F17},
        {"f18", VK_F18},
        {"f19", VK_F19},
        {"f20", VK_F20},
        {"f21", VK_F21},
        {"f22", VK_F22},
        {"f23", VK_F23},
        {"f24", VK_F24},

        {"a", 'A'},
        {"b", 'B'},
        {"c", 'C'},
        {"d", 'D'},
        {"e", 'E'},
        {"f", 'F'},
        {"g", 'G'},
        {"h", 'H'},
        {"i", 'I'},
        {"j", 'J'},
        {"k", 'K'},
        {"l", 'L'},
        {"m", 'M'},
        {"n", 'N'},
        {"o", 'O'},
        {"p", 'P'},
        {"q", 'Q'},
        {"r", 'R'},
        {"s", 'S'},
        {"t", 'T'},
        {"u", 'U'},
        {"v", 'V'},
        {"w", 'W'},
        {"x", 'X'},
        {"y", 'Y'},
        {"z", 'Z'},

        {"0", '0'},
        {"1", '1'},
        {"2", '2'},
        {"3", '3'},
        {"4", '4'},
        {"5", '5'},
        {"6", '6'},
        {"7", '7'},
        {"8", '8'},
        {"9", '9'},

        {"num0", VK_NUMPAD0},
        {"num1", VK_NUMPAD1},
        {"num2", VK_NUMPAD2},
        {"num3", VK_NUMPAD3},
        {"num4", VK_NUMPAD4},
        {"num5", VK_NUMPAD5},
        {"num6", VK_NUMPAD6},
        {"num7", VK_NUMPAD7},
        {"num8", VK_NUMPAD8},
        {"num9", VK_NUMPAD9},
        {"num_mul", VK_MULTIPLY},
        {"num_add", VK_ADD},
        {"num_sep", VK_SEPARATOR},
        {"num_sub", VK_SUBTRACT},
        {"num_dec", VK_DECIMAL},
        {"num_div", VK_DIVIDE},

        {"tab", VK_TAB},
        {"space", VK_SPACE},
        {"esc", VK_ESCAPE},
        {"enter", VK_RETURN},
        {"backspace", VK_BACK},
        {"ins", VK_INSERT},
        {"del", VK_DELETE},
        {"home", VK_HOME},
        {"end", VK_END},
        {"pgup", VK_PRIOR},
        {"pgdn", VK_NEXT},
        {"pause", VK_PAUSE},
        {"print", VK_SNAPSHOT},

        {"ctrl", VK_CONTROL},
        {"lctrl", VK_LCONTROL},
        {"rctrl", VK_RCONTROL},
        {"shift", VK_SHIFT},
        {"lshift", VK_LSHIFT},
        {"rshift", VK_RSHIFT},
        {"alt", VK_MENU},
        {"lalt", VK_LMENU},
        {"ralt", VK_RMENU},
        {"win", VK_LWIN},
        {"lwin", VK_LWIN},
        {"rwin", VK_RWIN},
        {"caps", VK_CAPITAL},

        {"up", VK_UP},
        {"down", VK_DOWN},
        {"left", VK_LEFT},
        {"right", VK_RIGHT},

        {"semicolon", VK_OEM_1},   // ;
        {"plus", VK_OEM_PLUS},     // +
        {"comma", VK_OEM_COMMA},   // ,
        {"minus", VK_OEM_MINUS},   // -
        {"period", VK_OEM_PERIOD}, // .
        {"slash", VK_OEM_2},       // /
        {"tilde", VK_OEM_3},       // ~
        {"lbracket", VK_OEM_4},    // [
        {"backslash", VK_OEM_5},   // backslash
        {"rbracket", VK_OEM_6},    // ]
        {"quote", VK_OEM_7}        // '
    };
    auto it = vk_map.find(sv);
    if (it == vk_map.end()) {
        spdlog::warn("Unknown keycode: {}", sv);
        return 0;
    }
    return it->second;
}

constexpr bool is_valid_vk(int vk) { return vk > 0; }

static conf::Task task_from_string(string_view sv) {
    static std::map<std::string_view, conf::Task> task_map = {
        {"save_state", conf::Task::SaveState}, {"load_state", conf::Task::LoadState}};
    auto it = task_map.find(sv);
    if (it == task_map.end()) {
        spdlog::warn("Unknown task: {}", sv);
        return conf::Task::None;
    }
    return it->second;
}

Config::Config() { fps = 60; }

void Config::read() {
    auto data = read_config_file();
    if (data["fps"].is_number_integer() && data["fps"].is_number_unsigned())
        fps = data["fps"];
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
                bind.key = vk_from_string(skey);
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
            bind.extra = 0;
            if (bind.task == Task::None)
                continue;
            if (val["mods"].is_array()) {
                for (auto& mod : val["mods"]) {
                    if (mod.is_string()) {
                        string skey = mod;
                        std::transform(skey.begin(), skey.end(), skey.begin(),
                                       [](int c) { return std::tolower(c); });
                        auto key = vk_from_string(skey);
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
            spdlog::debug("Bind (task={}, extra={}, key={}, mods={})", static_cast<int>(bind.task),
                          bind.extra, bind.key, bind.mods);
            binds.push_back(bind);
        }
        std::sort(binds.begin(), binds.end(),
                  [](const auto& a, const auto& b) { return a.key < b.key; });
    }
}

void conf::init() { _conf_ptr = new Config; }

Config& conf::get() {
    __assume(_conf_ptr != nullptr);
    return *_conf_ptr;
}
