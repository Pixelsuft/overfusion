#define WIN32_LEAN_AND_MEAN
#include "loadhooks.hpp"
#include "audio.hpp"
#include "config.hpp"
#include "d3d9hooks.hpp"
#include "extrahooks.hpp"
#include "mem.hpp"
#include "opt.hpp"
#include "plugbase.hpp"
#include "sv.hpp"
#include "uconv.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

using ost::optional;
using ost::string_view;
using std::string;

#if (defined(_MSC_VER) ? _MSVC_LANG : __cplusplus) >= 201703L
static constexpr
#else
static
#endif
    string_view get_filename(string_view path) noexcept {
    auto last_slash = path.find_last_of("/\\");
    if (last_slash == string_view::npos)
        return path;
    return path.substr(last_slash + 1);
}

static string get_module_path(HMODULE module) {
    wchar_t path[MAX_PATH];
    DWORD size = GetModuleFileNameW(module, path, MAX_PATH);
    if (size == 0) {
        spdlog::error("Failed to get module path, error code: {}", GetLastError());
        return "";
    }
    if (size >= MAX_PATH) {
        spdlog::error("Module path is too long");
        return "";
    }
    path[size] = L'\0';
    return uconv::from_utf16(path);
}

static optional<std::string> before_load(string_view path) {
    auto fn = get_filename(path);
    // Blocklist some unnecessary modules
    if (fn == "mmf2d3d8.dll") {
        spdlog::warn("Direct3D8 is not supported, a custom window will be used");
        // return "";
    } else if (fn == "Imm32.dll" || fn == "mscoree.dll" || fn == "uxtheme.dll") {
        return "";
    } else if (fn == "xinput1_4.dll" || fn == "xinput1_3.dll" || fn == "xinput1_2.dll" ||
               fn == "xinput1_1.dll" || fn == "xinput9_1_0.dll") {
        return "";
    }
    // spdlog::debug("Library loaded: {}", fn);
    return plug::get().before_dll_load(path, fn);
}

static void after_load(string_view path, void* mod) {
    auto fn = get_filename(path);
    if (mod) {
        if (fn == "mmfs2.dll") {
            d3d9hooks::pre_init();
            audio::init();
            hook::enable();
        } else if (fn == "mmf2d3d9.dll") {
            conf::get().custom_window = false;
            d3d9hooks::init();
            hook::enable();
        } else if (fn == "Lacewing.mfx") {
            extrahooks::init_ws32();
            hook::enable();
        } else if (fn == "Yaso.mfx") {
            extrahooks::init_inet();
            hook::enable();
        }
    }
    plug::get().after_dll_load(path, fn, mod);
    // Fully re-check IAT because DLL may have hidden deps
    hook::patch_iat();
}

static HMODULE(WINAPI* LoadLibraryAO)(LPCSTR lpLibFileName);
static HMODULE WINAPI LoadLibraryAH(LPCSTR lpLibFileName) {
    HMODULE ret = nullptr;
    auto orig_name = uconv::from_ansi(lpLibFileName);
    auto name = before_load(orig_name);
    if (!name.has_value()) {
        ret = LoadLibraryAO(lpLibFileName);
        after_load(orig_name, ret);
    } else if (name.value().empty())
        ret = nullptr;
    else {
        auto need_name = uconv::to_ansi(name.value());
        if (need_name) {
            ret = LoadLibraryAO(need_name);
            std::free(need_name);
            after_load(name.value(), ret);
        }
    }
    return ret;
}

static HMODULE(WINAPI* LoadLibraryWO)(LPCWSTR lpLibFileName);
static HMODULE WINAPI LoadLibraryWH(LPCWSTR lpLibFileName) {
    HMODULE ret = nullptr;
    auto orig_name = uconv::from_utf16(lpLibFileName);
    auto name = before_load(orig_name);
    if (!name.has_value()) {
        ret = LoadLibraryWO(lpLibFileName);
        after_load(orig_name, ret);
    } else if (name.value().empty())
        ret = nullptr;
    else {
        auto need_name = uconv::to_utf16(name.value());
        if (need_name) {
            ret = LoadLibraryWO(need_name);
            std::free(need_name);
            after_load(name.value(), ret);
        }
    }
    return ret;
}

static FARPROC(WINAPI* GetProcAddressO)(HMODULE hModule, LPCSTR lpProcName) = GetProcAddress;
static FARPROC WINAPI GetProcAddressH(HMODULE hModule, LPCSTR lpProcName) {
    if (!hModule || !lpProcName)
        return GetProcAddressO(hModule, lpProcName);
    void* temp_ret = (reinterpret_cast<ULONG_PTR>(lpProcName) > 0xFFFF)
                         ? hook::get_iated_func(hModule, lpProcName)
                         : nullptr;
    if (temp_ret == nullptr) {
        temp_ret = reinterpret_cast<void*>(GetProcAddressO(hModule, lpProcName));
    } else {
        spdlog::warn("GetProcAddress was used to get a hooked function addr: {}", lpProcName);
    }
    temp_ret = plug::get().after_proc_get(hModule, lpProcName, temp_ret);
    if (reinterpret_cast<ULONG_PTR>(lpProcName) > 0xFFFF) {
        string_view proc(lpProcName);
        if (proc == "SaveRunObject" && !temp_ret) {
            string path = get_module_path(hModule);
            spdlog::warn("This object does not support state save/load: {}", get_filename(path));
        } else if (proc == "SaveRunObject" &&
                   GetProcAddressO(hModule, "LoadRunObject") == nullptr) {
            string path = get_module_path(hModule);
            spdlog::warn("This object does support state save but doesn't support load (WHAT?): {}",
                         get_filename(path));
        }
        // spdlog::debug("GetProcAddress: {}", lpProcName);
    }
    return reinterpret_cast<FARPROC>(temp_ret);
}

void loadhooks::init() {
    IAT_STR_AUTO("kernel32.dll", LoadLibrary);
    IAT_AUTO("kernel32.dll", GetProcAddress);
    // For really old MMF2 versions
    auto handle = GetModuleHandleW(L"mmfs2.dll");
    if (handle != nullptr)
        after_load("mmfs2.dll", handle);
    handle = GetModuleHandleW(L"mmf2d3d9.dll");
    if (handle != nullptr)
        after_load("mmf2d3d9.dll", handle);
}

void* loadhooks::get_func_address(void* handle, const char* func_name) {
    return reinterpret_cast<void*>(GetProcAddressO(reinterpret_cast<HMODULE>(handle), func_name));
}
