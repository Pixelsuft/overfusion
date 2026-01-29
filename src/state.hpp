#pragma once
#include "ofs.hpp"
#include <cstdint>

namespace state {
    enum class TimeOffset {
        None,
        System,
        Local,
        Startup,
        Reminder
    };

    void init();
    void invalidate_process();
    void after_update();
    bool save(ofs::File& file);
    bool load(ofs::File& file);
    uint64_t get_time(TimeOffset offset);
    int64_t get_utc_offset();
}
