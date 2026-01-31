#pragma once
#include <string>
#include "sv.hpp"

namespace ofs {
    enum SeekMode {
		SeekBegin = 0,
		SeekCurrent = 1,
		SeekEnd = 2
	};

	class File {
	private:
		void* handle;
	public:
		File() noexcept;
		File(ost::string_view path, int mode) noexcept;
		bool open(ost::string_view path, int mode);
		bool is_open();
		bool read_line(std::string& line);
		bool read(void* buf, size_t size);
		bool write(const void* buf, size_t size);
		inline bool write(ost::string_view data) {
			return write(data.data(), data.size());
		}
		inline bool write_line(ost::string_view line) {
			return write(line.data(), line.size()) && write("\r\n", 2);
		}
		bool seek(long long offset, SeekMode mode);
		long long tell();
		long long size();
		inline void* get_handle() {
			return handle;
		}
		void close();
		inline ~File() {
			close();
		}
		File(const File&) = delete;
		File& operator=(const File&) = delete;
		File(File&& other) noexcept;
		File& operator=(File&& other) noexcept;
	};
}
