#pragma once
#include "sv.hpp"
#include <string>

namespace ofs {
enum SeekMode { SeekBegin = 0, SeekCurrent = 1, SeekEnd = 2 };

class File {
private:
    void* handle;

public:
    File() noexcept;
    File(of::string_view path, int mode) noexcept;
    bool open(of::string_view path, int mode, bool hooked = false);
    bool is_open();
    bool readln(std::string& line);
    bool read(void* buf, size_t size);
    bool write(const void* buf, size_t size);
    inline bool write(of::string_view data) { return write(data.data(), data.size()); }
    inline bool writeln(of::string_view line) {
        return write(line.data(), line.size()) && write("\r\n", 2);
    }
    bool seek(long long offset, SeekMode mode);
    long long tell();
    long long size();
    inline void* get_handle() { return handle; }
    void close();
    inline ~File() { close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;
};

void pre_init();
of::string_view get_cwd();
bool remove_file(of::string_view path);
bool make_dir(of::string_view path);
bool file_exists(of::string_view path);
} // namespace ofs
