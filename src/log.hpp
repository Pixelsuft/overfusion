#pragma once
#ifndef NO_SPDLOG
#include <spdlog/spdlog.h>
#else
#include <iostream>
#include <sstream>
#endif

namespace of {
#ifndef NO_SPDLOG

using spdlog::debug;
using spdlog::error;
using spdlog::info;
using spdlog::warn;

#else

inline void format_helper(std::ostringstream& oss, const std::string& fmt, size_t pos) {
    oss << fmt.substr(pos);
}

template <typename T, typename... Args>
void format_helper(std::ostringstream& oss, const std::string& fmt, size_t pos, T&& value,
                   Args&&... args) {
    size_t placeholder = fmt.find("{}", pos);

    if (placeholder == std::string::npos) {
        oss << fmt.substr(pos);
        return;
    }

    oss << fmt.substr(pos, placeholder - pos);
    oss << std::forward<T>(value);

    format_helper(oss, fmt, placeholder + 2, std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_format(const char* level, const std::string& fmt, Args&&... args) {
    std::ostringstream oss;
    format_helper(oss, fmt, 0, std::forward<Args>(args)...);

    std::cout << "[" << level << "] " << oss.str() << std::endl;
}

template <typename... Args> void debug(const std::string& fmt, Args&&... args) {
    log_format("debug", fmt, std::forward<Args>(args)...);
}

template <typename... Args> void info(const std::string& fmt, Args&&... args) {
    log_format("info", fmt, std::forward<Args>(args)...);
}

template <typename... Args> void warn(const std::string& fmt, Args&&... args) {
    log_format("warning", fmt, std::forward<Args>(args)...);
}

template <typename... Args> void error(const std::string& fmt, Args&&... args) {
    log_format("error", fmt, std::forward<Args>(args)...);
}

#endif
} // namespace of
