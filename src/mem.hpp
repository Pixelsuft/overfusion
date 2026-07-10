#pragma once
#include "sv.hpp"
#include <initializer_list>
#include <string>
#include <type_traits>

// Normal hooking
#define HOOK_AUTO(lib, func) hook::hook(mem::get_addr(lib, #func), func##H, &func##O)
#define HOOK_ONLY(lib, func) hook::hook(mem::get_addr(lib, #func), func##H)
#define HOOK_STR_AUTO(lib, func)                                                                   \
    (hook::hook(mem::get_addr(lib, #func "W"), func##WH, &func##WO) &&                             \
     hook::hook(mem::get_addr(lib, #func "A"), func##AH, &func##AO))
#define HOOK_STR_ONLY(lib, func)                                                                   \
    (hook::hook(mem::get_addr(lib, #func "W"), func##WH) &&                                        \
     hook::hook(mem::get_addr(lib, #func "A"), func##AH))

// IAT hooking
#define IAT_AUTO(lib, func) hook::iat_reg(lib, #func, func##H, &func##O)
#define IAT_ONLY(lib, func) hook::iat_reg(lib, #func, func##H)
#define IAT_STR_AUTO(lib, func)                                                                    \
    (hook::iat_reg(lib, #func "W", func##WH, &func##WO) &&                                         \
     hook::iat_reg(lib, #func "A", func##AH, &func##AO))
#define IAT_STR_ONLY(lib, func)                                                                    \
    (hook::iat_reg(lib, #func "W", func##WH) && hook::iat_reg(lib, #func "A", func##AH))
#define IAT_ORD_AUTO(lib, ord) hook::iat_reg_ord(lib, ord, Ordinal_##ord##H, &Ordinal_##ord##O)
#define IAT_ORD_ONLY(lib, ord) hook::iat_reg_ord(lib, ord, Ordinal_##ord##H)

namespace mem {
extern std::string exe_name;
bool _write_memory(size_t addr, const void* data, size_t size);
bool _flush_instructions(size_t addr, size_t size);
void init();
void terminate();
size_t get_base();
size_t get_base(const char* obj_name);
void* get_addr(const char* obj_name, const char* func_name);
template <bool Flush = false> inline bool write(size_t addr, std::initializer_list<uint8_t> bytes) {
    bool result = _write_memory(addr, bytes.begin(), bytes.size());
    if (result && Flush)
        result = result && _flush_instructions(addr, bytes.size());
    return result;
}

#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 202002L
template <bool Flush = false, typename T>
    requires std::is_trivially_copyable_v<T>
bool write(size_t addr, const T& value) {
    bool result = _write_memory(addr, std::addressof(value), sizeof(T));
    if (result && Flush)
        result = result && _flush_instructions(addr, sizeof(T));
    return result;
}
#else
template <bool Flush = false, typename T> bool write(size_t addr, const T& value) {
    bool result = _write_memory(addr, std::addressof(value), sizeof(T));
    if (result && Flush)
        result = result && _flush_instructions(addr, sizeof(T));
    return result;
}
#endif
} // namespace mem

namespace hook {
bool _enable_target(void* target);
bool _hook_target(void* pTarget, void* pDetour, void** ppOriginal);
void _patch_vtable(void** vtable, int index, void* new_func, void** old_func);
bool _reg_iat(of::string_view dll, of::string_view func_name, int ordinal, void* pNewFunc,
              void** ppOriginal);
bool patch_iat();
void* get_iated_func(void* mod, of::string_view name);

template <typename A, typename F> inline bool hook(A pTarget, F* pDetour) {
    return _hook_target(reinterpret_cast<void*>(pTarget), reinterpret_cast<void*>(pDetour),
                        nullptr);
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
    _patch_vtable(vtable, index, reinterpret_cast<void*>(new_func),
                  reinterpret_cast<void**>(old_func));
}

template <typename F> inline bool iat_reg(of::string_view dll, of::string_view func, F* pDetour) {
    return _reg_iat(dll, func, 0, reinterpret_cast<void*>(pDetour), nullptr);
}

template <typename F, typename T>
inline bool iat_reg(of::string_view dll, of::string_view func, F* pDetour, T** ppOriginal) {
    return _reg_iat(dll, func, 0, reinterpret_cast<void*>(pDetour),
                    reinterpret_cast<void**>(ppOriginal));
}

template <typename F> inline bool iat_reg_ord(of::string_view dll, int ord, F* pDetour) {
    return _reg_iat(dll, "", ord, reinterpret_cast<void*>(pDetour), nullptr);
}

template <typename F, typename T>
inline bool iat_reg_ord(of::string_view dll, int ord, F* pDetour, T** ppOriginal) {
    return _reg_iat(dll, "", ord, reinterpret_cast<void*>(pDetour),
                    reinterpret_cast<void**>(ppOriginal));
}

inline bool enable() { return _enable_target(nullptr); }
} // namespace hook
