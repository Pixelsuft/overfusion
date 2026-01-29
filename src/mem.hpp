#pragma once
#include <initializer_list>
#include <string>

#define HOOK_AUTO(lib, func) hook::hook(mem::get_addr(lib, #func), func##H, &func##O)
#define HOOK_ONLY(lib, func) hook::hook(mem::get_addr(lib, #func), func##H)

namespace mem {
extern std::string exe_name;
void _write_memory(size_t addr, const void* data, size_t size);
void init();
void terminate();
size_t get_base();
size_t get_base(const char* obj_name);
void* get_addr(const char* obj_name, const char* func_name);
inline void write(size_t addr, std::initializer_list<uint8_t> bytes) {
    _write_memory(addr, bytes.begin(), bytes.size());
}
template <typename T>
    requires std::is_trivially_copyable_v<T>
void write(size_t addr, const T& value) {
    _write_memory(addr, std::addressof(value), sizeof(T));
}
} // namespace mem

namespace hook {
void _enable_target(void* target);
void _hook_target(void* pTarget, void* pDetour, void** ppOriginal);

template <typename A, typename F> inline void hook(A pTarget, F* pDetour) {
    _hook_target(reinterpret_cast<void*>(pTarget), reinterpret_cast<void*>(pDetour), nullptr);
}

template <typename A, typename F, typename T>
inline void hook(A pTarget, F* pDetour, T** ppOriginal) {
    _hook_target(reinterpret_cast<void*>(pTarget), reinterpret_cast<void*>(pDetour),
                 reinterpret_cast<void**>(ppOriginal));
}

template <typename T> inline void enable(T target) {
    _enable_target(reinterpret_cast<void*>(target));
}

inline void enable() { _enable_target(nullptr); }
} // namespace hook
