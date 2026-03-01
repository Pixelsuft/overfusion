#define WIN32_LEAN_AND_MEAN
#include "input.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "mem.hpp"
#include "state.hpp"
#include "ui.hpp"
#include <Windows.h>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <vector>

static bool kbd_state[256];
static std::vector<std::pair<int, bool>> kbd_que;

static SHORT(WINAPI* GetAsyncKeyStateO)(int nVirtKey);
static SHORT WINAPI GetAsyncKeyStateH(int nVirtKey) {
    if (nVirtKey < 0 || nVirtKey >= 256) {
        spdlog::warn("Invalid nVirtKey for GetAsyncKeyState");
        return 0;
    }
    // not used by imgui (for now)
    // if (ui::processing)
    //     return GetAsyncKeyStateO(nVirtKey);
    return state::get_key_state(nVirtKey) ? -32767 : 0;
}

static SHORT(WINAPI* GetKeyStateO)(int nVirtKey);
static SHORT WINAPI GetKeyStateH(int nVirtKey) {
    if (nVirtKey < 0 || nVirtKey >= 256) {
        spdlog::warn("Invalid nVirtKey for GetKeyState");
        return 0;
    }
    if (ui::processing)
        return kbd_state[nVirtKey];
    return state::get_key_state(nVirtKey) ? -32767 : 0;
}

static BOOL(WINAPI* GetCursorPosO)(LPPOINT lpPoint);
static BOOL WINAPI GetCursorPosH(LPPOINT lpPoint) {
    if (ui::processing)
        return GetCursorPosO(lpPoint);
    // TODO
    lpPoint->x = -100;
    lpPoint->y = -100;
    return TRUE;
}

static BOOL WINAPI GetInputStateH() {
    // Is this proper?
    return FALSE;
}

static HKL(WINAPI* GetKeyboardLayoutO)(DWORD idThread);
static HKL WINAPI GetKeyboardLayoutH(DWORD idThread) {
    if (ui::processing)
        return GetKeyboardLayoutO(idThread);
    return reinterpret_cast<HKL>(0x04090409);
}

static BOOL WINAPI GetKeyboardStateH(PBYTE lpKeyState) {
    // spdlog::info("GetKeyboardState");
    memset(lpKeyState, 0, sizeof(BYTE) * 256);
    state::fill_kbd_state(lpKeyState);
    return TRUE;
}

static BOOL WINAPI SetKeyboardStateH(LPBYTE lpKeyState) {
    spdlog::info("SetKeyboardState");
    return FALSE;
}

static BOOL WINAPI OpenClipboardH(HWND hWndNewOwner) {
    spdlog::info("OpenClipboard");
    return FALSE;
}

static BOOL WINAPI IsClipboardFormatAvailableH(UINT format) { return FALSE; }

static VOID WINAPI keybd_eventH(BYTE bVk, BYTE bScan, DWORD dwFlags, ULONG_PTR dwExtraInfo) {}

static VOID WINAPI mouse_eventH(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData,
                                ULONG_PTR dwExtraInfo) {}

static UINT WINAPI SendInputH(UINT cInputs, LPINPUT pInputs, int cbSize) { return 0; }

static BOOL WINAPI SetCursorPosH(int X, int Y) { return FALSE; }

void input::init() {
    std::memset(kbd_state, 0, sizeof(bool) * 256);
    HOOK_AUTO("user32.dll", GetKeyState);
    HOOK_AUTO("user32.dll", GetAsyncKeyState);
    HOOK_AUTO("user32.dll", GetCursorPos);
    HOOK_ONLY("user32.dll", GetInputState);
    HOOK_AUTO("user32.dll", GetKeyboardLayout);
    HOOK_ONLY("user32.dll", GetKeyboardState);
    HOOK_ONLY("user32.dll", SetKeyboardState);
    HOOK_ONLY("user32.dll", OpenClipboard);
    HOOK_ONLY("user32.dll", IsClipboardFormatAvailable);
    HOOK_ONLY("user32.dll", keybd_event);
    HOOK_ONLY("user32.dll", mouse_event);
    HOOK_ONLY("user32.dll", SendInput);
    HOOK_ONLY("user32.dll", SetCursorPos);
}

void input::handle_input(int vk, bool pressed) { kbd_que.push_back({vk, pressed}); }

void input::process_update() {
    for (auto& val : kbd_que)
        handle_input_real(val.first, val.second);
    kbd_que.clear();
}

void input::handle_input_real(int vk, bool pressed) {
    ASS(vk > 0 && vk < 256);
    auto& cfg = conf::get();
    auto it = std::lower_bound(cfg.binds.begin(), cfg.binds.end(), vk,
                               [](const auto& a, int key) { return a.key > key; });
    bool prev = kbd_state[vk];
    kbd_state[vk] = pressed;
    while (it != cfg.binds.end() && it->key == vk) {
        auto& bind = *it;
        bool matches_mod = true;
        for (auto& mod : bind.mods) {
            if (!kbd_state[mod]) {
                matches_mod = false;
                break;
            }
        }
        if ((pressed && !matches_mod) || (!pressed && !prev)) {
            it++;
            continue;
        }
        switch (bind.task) {
        case conf::Task::SaveState: {
            if (pressed && !prev && !cfg.show_menu)
                state::save_state(bind.extra);
            break;
        }
        case conf::Task::LoadState: {
            if (pressed && !prev && !cfg.show_menu)
                state::load_state(bind.extra);
            break;
        }
        case conf::Task::Advance: {
            if (pressed && !cfg.show_menu) {
                cfg.need_advance = true;
                cfg.is_paused = true;
            }
            break;
        }
        case conf::Task::Play: {
            if (pressed && cfg.show_menu)
                break;
            if (!bind.extra)
                cfg.is_paused = !pressed;
            else if (pressed)
                cfg.is_paused = !cfg.is_paused;
            break;
        }
        case conf::Task::FastForward: {
            if (pressed && cfg.show_menu)
                break;
            if (!bind.extra)
                cfg.fast_forward = pressed;
            else if (pressed)
                cfg.fast_forward = !cfg.fast_forward;
            break;
        }
        case conf::Task::Map: {
            if (pressed && (prev || cfg.show_menu))
                break;
            state::set_key_down(vk, pressed);
            break;
        }
        case conf::Task::Menu: {
            if (pressed)
                cfg.show_menu = !cfg.show_menu;
            break;
        }
        case conf::Task::None: {
            ASS(false);
            break;
        }
        default: {
            ASS(false);
            break;
        }
        }
        it++;
    }
}
