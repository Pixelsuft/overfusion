#pragma once

namespace conf {
class Config {
public:
    int fps;

    Config();
    void read();
};

void init();
Config& get();
} // namespace config
