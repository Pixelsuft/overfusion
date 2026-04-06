#pragma once
#include <initializer_list>
#include <type_traits>
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
#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 202002L
template <typename T>
    requires std::is_trivially_copyable_v<T>
bool write(size_t addr, const T& value) {
    return _write_memory(addr, std::addressof(value), sizeof(T));
}
#else
template <typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
bool write(size_t addr, const T& value) {
    return _write_memory(addr, std::addressof(value), sizeof(T));
}
#endif
} // namespace mem

namespace hook {
bool _enable_target(void* target);
bool _hook_target(void* pTarget, void* pDetour, void** ppOriginal);
void _patch_vtable(void** vtable, int index, void* new_func, void** old_func);

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

template <typename A, typename B>
inline void patch_vtable(void** vtable, int index, A* new_func, B** old_func) {
    _patch_vtable(vtable, index, reinterpret_cast<void*>(new_func), reinterpret_cast<void**>(old_func));
}

inline bool enable() { return _enable_target(nullptr); }
} // namespace hook
