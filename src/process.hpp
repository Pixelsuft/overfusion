#pragma once
#include "sv.hpp"
#include "opt.hpp"
#include <Windows.h>
#include <string>

// Minimal subprocess library

namespace process {
class Subprocess {
private:
    PROCESS_INFORMATION pi;
    HANDLE hChildStdinRead;
    HANDLE hChildStdinWrite;

public:
    Subprocess();
    bool is_open();
    bool open(of::string_view cmdline);
    bool close();
    bool write(const void* data, size_t size);
    Subprocess(const Subprocess&) = delete;
    Subprocess& operator=(const Subprocess&) = delete;
    Subprocess(Subprocess&&) = delete;
    Subprocess& operator=(Subprocess&&) = delete;
    ~Subprocess();
};

of::optional<std::string> get_env(of::string_view key);
} // namespace process
