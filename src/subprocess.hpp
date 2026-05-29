#pragma once
#include "sv.hpp"
#include <Windows.h>

// Minimal subprocess library

namespace subprocess {
class Process {
private:
    PROCESS_INFORMATION pi;
    HANDLE hChildStdinRead;
    HANDLE hChildStdinWrite;

public:
    Process();
    bool is_open();
    bool open(ost::string_view cmdline);
    bool close();
    bool write(const void* data, size_t size);
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    Process(Process&&) = delete;
    Process& operator=(Process&&) = delete;
    ~Process();
};
} // namespace subprocess
