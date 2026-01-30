#define WIN32_LEAN_AND_MEAN
#include "config.hpp"
#include "filehooks.hpp"
#include "ofs.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using conf::Config;

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
    if (size < 2 || size > (1024 * 1024)) {
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

Config::Config() {
    fps = 60;
}

void Config::read() {
    auto data = read_config_file();
    if (data["fps"].is_number_integer() && data["fps"].is_number_unsigned())
        fps = data["fps"];
}

void conf::init() { _conf_ptr = new Config; }

Config& conf::get() {
    __assume(_conf_ptr != nullptr);
    return *_conf_ptr;
}
