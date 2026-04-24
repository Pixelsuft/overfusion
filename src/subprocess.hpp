#pragma once
#include <Windows.h>
#include "sv.hpp"

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
    ~Process();
};
} // namespace subprocess
