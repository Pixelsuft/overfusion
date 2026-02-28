#pragma once
#include <initializer_list>
#include <string>

#define HOOK_AUTO(lib, func) hook::hook(mem::get_addr(lib, #func), func##H, &func##O)
#define HOOK_ONLY(lib, func) hook::hook(mem::get_addr(lib, #func), func##H)
#define HOOK_STR_AUTO(lib, func) (hook::hook(mem::get_addr(lib, #func "W"), func##WH, &func##WO) && hook::hook(mem::get_addr(lib, #func "A"), func##AH, &func##AO))
#define HOOK_STR_ONLY(lib, func) (hook::hook(mem::get_addr(lib, #func "W"), func##WH) && hook::hook(mem::get_addr(lib, #func "A"), func##AH))

namespace mem {
extern std::string exe_name;
bool _write_memory(size_t addr, const void* data, size_t size);
void init();
void terminate();
size_t get_base();
size_t get_base(const char* obj_name);
void* get_addr(const char* obj_name, const char* func_name);
inline bool write(size_t addr, std::initializer_list<uint8_t> bytes) {
    return _write_memory(addr, bytes.begin(), bytes.size());
}
template <typename T>
    requires std::is_trivially_copyable_v<T>
bool write(size_t addr, const T& value) {
    return _write_memory(addr, std::addressof(value), sizeof(T));
}
} // namespace mem

namespace hook {
bool _enable_target(void* target);
bool _hook_target(void* pTarget, void* pDetour, void** ppOriginal);

template <typename A, typename F> inline bool hook(A pTarget, F* pDetour) {
    return _hook_target(reinterpret_cast<void*>(pTarget), reinterpret_cast<void*>(pDetour), nullptr);
}

template <typename A, typename F, typename T>
inline bool hook(A pTarget, F* pDetour, T** ppOriginal) {
    return _hook_target(reinterpret_cast<void*>(pTarget), reinterpret_cast<void*>(pDetour),
                 reinterpret_cast<void**>(ppOriginal));
}

template <typename T> inline bool enable(T target) {
    return _enable_target(reinterpret_cast<void*>(target));
}

inline bool enable() { return _enable_target(nullptr); }
} // namespace hook
