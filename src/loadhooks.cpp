#define WIN32_LEAN_AND_MEAN
#include "loadhooks.hpp"
#include "audiohooks.hpp"
#include "d3d9hooks.hpp"
#include "extrahooks.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "uconv.hpp"
#include <Windows.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <string_view>

using std::string, std::string_view;

constexpr string_view get_filename(string_view path) noexcept {
    auto last_slash = path.find_last_of("/\\");
    if (last_slash == string_view::npos)
        return path;
    return path.substr(last_slash + 1);
}

static std::optional<std::string> before_load(string_view path) {
    auto fn = get_filename(path);
    if (fn == "mmf2d3d8.dll") {
        spdlog::error("Direct3D8 is not supported yet, failing to load it");
        return "";
    } else if (fn == "Imm32.dll" || fn == "mscoree.dll" || fn == "uxtheme.dll") {
        return "";
    } else if (fn == "xinput1_4.dll" || fn == "xinput1_3.dll" || fn == "xinput1_2.dll" ||
               fn == "xinput1_1.dll" || fn == "xinput9_1_0.dll") {
        return "";
    }
    return {};
}

static void after_load(string_view path, void* mod) {
    auto fn = get_filename(path);
    if (mod) {
        if (fn == "mmfs2.dll") {
            audiohooks::init();
            hook::enable();
        } else if (fn == "mmf2d3d9.dll") {
            d3d9hooks::init();
            hook::enable();
        } else if (fn == "Lacewing.mfx") {
            extrahooks::init_net();
            hook::enable();
        }
    }
    plug::get().after_dll_load(path, fn, mod);
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

void loadhook::init() {
    HOOK_AUTO("kernel32.dll", LoadLibraryA);
    HOOK_AUTO("kernel32.dll", LoadLibraryW);
}
