#define WIN32_LEAN_AND_MEAN
#include "input.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "sv.hpp"
#include "ui.hpp"
#include <Windows.h>
#include <algorithm>
#include <map>
#include <spdlog/spdlog.h>
#include <vector>

namespace input {
static bool kbd_state[256];
static std::vector<std::pair<int, bool>> kbd_que;

static const std::map<int, ost::string_view> vk_map = {{VK_F1, "f1"},
                                                       {VK_F2, "f2"},
                                                       {VK_F3, "f3"},
                                                       {VK_F4, "f4"},
                                                       {VK_F5, "f5"},
                                                       {VK_F6, "f6"},
                                                       {VK_F7, "f7"},
                                                       {VK_F8, "f8"},
                                                       {VK_F9, "f9"},
                                                       {VK_F10, "f10"},
                                                       {VK_F11, "f11"},
                                                       {VK_F12, "f12"},
                                                       {VK_F13, "f13"},
                                                       {VK_F14, "f14"},
                                                       {VK_F15, "f15"},
                                                       {VK_F16, "f16"},
                                                       {VK_F17, "f17"},
                                                       {VK_F18, "f18"},
                                                       {VK_F19, "f19"},
                                                       {VK_F20, "f20"},
                                                       {VK_F21, "f21"},
                                                       {VK_F22, "f22"},
                                                       {VK_F23, "f23"},
                                                       {VK_F24, "f24"},

                                                       {'A', "a"},
                                                       {'B', "b"},
                                                       {'C', "c"},
                                                       {'D', "d"},
                                                       {'E', "e"},
                                                       {'F', "f"},
                                                       {'G', "g"},
                                                       {'H', "h"},
                                                       {'I', "i"},
                                                       {'J', "j"},
                                                       {'K', "k"},
                                                       {'L', "l"},
                                                       {'M', "m"},
                                                       {'N', "n"},
                                                       {'O', "o"},
                                                       {'P', "p"},
                                                       {'Q', "q"},
                                                       {'R', "r"},
                                                       {'S', "s"},
                                                       {'T', "t"},
                                                       {'U', "u"},
                                                       {'V', "v"},
                                                       {'W', "w"},
                                                       {'X', "x"},
                                                       {'Y', "y"},
                                                       {'Z', "z"},

                                                       {'0', "0"},
                                                       {'1', "1"},
                                                       {'2', "2"},
                                                       {'3', "3"},
                                                       {'4', "4"},
                                                       {'5', "5"},
                                                       {'6', "6"},
                                                       {'7', "7"},
                                                       {'8', "8"},
                                                       {'9', "9"},

                                                       {VK_NUMPAD0, "num0"},
                                                       {VK_NUMPAD1, "num1"},
                                                       {VK_NUMPAD2, "num2"},
                                                       {VK_NUMPAD3, "num3"},
                                                       {VK_NUMPAD4, "num4"},
                                                       {VK_NUMPAD5, "num5"},
                                                       {VK_NUMPAD6, "num6"},
                                                       {VK_NUMPAD7, "num7"},
                                                       {VK_NUMPAD8, "num8"},
                                                       {VK_NUMPAD9, "num9"},
                                                       {VK_MULTIPLY, "num_mul"},
                                                       {VK_ADD, "num_add"},
                                                       {VK_SEPARATOR, "num_sep"},
                                                       {VK_SUBTRACT, "num_sub"},
                                                       {VK_DECIMAL, "num_dec"},
                                                       {VK_DIVIDE, "num_div"},

                                                       {VK_TAB, "tab"},
                                                       {VK_SPACE, "space"},
                                                       {VK_ESCAPE, "esc"},
                                                       {VK_RETURN, "enter"},
                                                       {VK_BACK, "backspace"},
                                                       {VK_INSERT, "insert"},
                                                       {VK_DELETE, "del"},
                                                       {VK_HOME, "home"},
                                                       {VK_END, "end"},
                                                       {VK_PRIOR, "pgup"},
                                                       {VK_NEXT, "pgdn"},
                                                       {VK_PAUSE, "pause"},
                                                       {VK_SNAPSHOT, "print"},

                                                       {VK_CONTROL, "ctrl"},
                                                       {VK_LCONTROL, "lctrl"},
                                                       {VK_RCONTROL, "rctrl"},
                                                       {VK_SHIFT, "shift"},
                                                       {VK_LSHIFT, "lshift"},
                                                       {VK_RSHIFT, "rshift"},
                                                       {VK_MENU, "alt"},
                                                       {VK_LMENU, "lalt"},
                                                       {VK_RMENU, "ralt"},
                                                       {VK_LWIN, "lwin"},
                                                       {VK_RWIN, "rwin"},
                                                       {VK_CAPITAL, "caps"},

                                                       {VK_UP, "up"},
                                                       {VK_DOWN, "down"},
                                                       {VK_LEFT, "left"},
                                                       {VK_RIGHT, "right"},

                                                       {VK_LBUTTON, "lbutton"},
                                                       {VK_MBUTTON, "mbutton"},
                                                       {VK_RBUTTON, "rbutton"},

                                                       {VK_OEM_1, "semicolon"},
                                                       {VK_OEM_PLUS, "plus"},
                                                       {VK_OEM_COMMA, "comma"},
                                                       {VK_OEM_MINUS, "minus"},
                                                       {VK_OEM_PERIOD, "period"},
                                                       {VK_OEM_2, "slash"},
                                                       {VK_OEM_3, "tilde"},
                                                       {VK_OEM_4, "lbracket"},
                                                       {VK_OEM_5, "backslash"},
                                                       {VK_OEM_6, "rbracket"},
                                                       {VK_OEM_7, "quote"}};
} // namespace input

extern HWND hwnd;
extern HWND mhwnd;

extern LRESULT(__stdcall* MainWindowProcO)(HWND, UINT, WPARAM, LPARAM);
extern LRESULT(__stdcall* EditWindowProcO)(HWND, UINT, WPARAM, LPARAM);

static SHORT(WINAPI* GetAsyncKeyStateO)(int nVirtKey);
static SHORT WINAPI GetAsyncKeyStateH(int nVirtKey) {
    if (nVirtKey < 0 || nVirtKey >= 256) {
        spdlog::warn("Invalid nVirtKey for GetAsyncKeyState");
        return 0;
    }
    // not used by imgui (for now)
    // if (ui::is_processing())
    //     return GetAsyncKeyStateO(nVirtKey);
    return state::get_key_state(nVirtKey) ? -32767 : 0;
}

static SHORT(WINAPI* GetKeyStateO)(int nVirtKey);
static SHORT WINAPI GetKeyStateH(int nVirtKey) {
    if (nVirtKey < 0 || nVirtKey >= 256) {
        spdlog::warn("Invalid nVirtKey for GetKeyState");
        return 0;
    }
    if (ui::is_processing())
        return input::kbd_state[nVirtKey];
    return state::get_key_state(nVirtKey) ? -32767 : 0;
}

static BOOL(WINAPI* GetCursorPosO)(LPPOINT lpPoint);
static BOOL WINAPI GetCursorPosH(LPPOINT lpPoint) {
    if (ui::is_processing())
        return GetCursorPosO(lpPoint);
    auto mouse_pos = state::get_mouse_pos();
    lpPoint->x = mouse_pos.first;
    lpPoint->y = mouse_pos.second;
    return TRUE;
}

static BOOL WINAPI GetInputStateH() {
    // Is this proper?
    return FALSE;
}

static HKL(WINAPI* GetKeyboardLayoutO)(DWORD idThread);
static HKL WINAPI GetKeyboardLayoutH(DWORD idThread) {
    if (ui::is_processing())
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
    spdlog::warn("failing OpenClipboard");
    // TODO: emulation for iwbtg support
    return FALSE;
}

static BOOL WINAPI IsClipboardFormatAvailableH(UINT format) { return FALSE; }

static VOID WINAPI keybd_eventH(BYTE bVk, BYTE bScan, DWORD dwFlags, ULONG_PTR dwExtraInfo) {
    spdlog::info("keybd_event (vk={}, scan={}, flags={})", bVk, bScan, dwFlags);
}

static VOID WINAPI mouse_eventH(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData,
                                ULONG_PTR dwExtraInfo) {
    spdlog::info("mouse_event (delta=({}, {}), flags={})", dx, dy, dwFlags);
}

static UINT WINAPI SendInputH(UINT cInputs, LPINPUT pInputs, int cbSize) {
    spdlog::info("SendInput");
    return 0;
}

static BOOL WINAPI SetCursorPosH(int X, int Y) {
    spdlog::info("SetCursorPos({}, {})", X, Y);
    if (0) {
        // TODO: option to allow set cursor pos???
        auto mouse_pos = state::get_mouse_pos();
        mouse_pos.first = X;
        mouse_pos.second = Y;
        return TRUE;
    }
    return FALSE;
}

void input::init() {
    std::memset(kbd_state, 0, sizeof(bool) * 256);
    HOOK_AUTO("user32.dll", GetKeyState);
    HOOK_AUTO("user32.dll", GetAsyncKeyState);
    HOOK_AUTO("user32.dll", GetCursorPos);
    HOOK_ONLY("user32.dll", GetInputState);
    HOOK_AUTO("user32.dll", GetKeyboardLayout);
    HOOK_ONLY("user32.dll", GetKeyboardState);
    HOOK_ONLY("user32.dll", SetKeyboardState);
    // HOOK_ONLY("user32.dll", OpenClipboard);
    HOOK_ONLY("user32.dll", IsClipboardFormatAvailable);
    HOOK_ONLY("user32.dll", keybd_event);
    HOOK_ONLY("user32.dll", mouse_event);
    HOOK_ONLY("user32.dll", SendInput);
    HOOK_ONLY("user32.dll", SetCursorPos);
}

ost::optional<int> input::vk_from_string(ost::string_view s) {
    auto it = std::find_if(vk_map.begin(), vk_map.end(),
                           [&s](const auto& pair) { return pair.second == s; });
    if (it == vk_map.end()) {
        spdlog::warn("Unknown keycode string: {}", s);
        return {};
    }
    ASS(it->first != 0);
    return it->first;
}

ost::optional<ost::string_view> input::vk_to_string(int vk) {
    auto it = vk_map.find(vk);
    if (it == vk_map.end()) {
        spdlog::warn("Unknown keycode value: {}", vk);
        return {};
    }
    return it->second;
}

void input::handle_input(int vk, bool pressed) {
    if (1)
        kbd_que.push_back({vk, pressed});
    else
        handle_input_real(vk, pressed);
}

void input::process_update() {
    for (auto& val : kbd_que)
        handle_input_real(val.first, val.second);
    kbd_que.clear();
}

void input::handle_input_real(int vk, bool pressed) {
    ASS(vk > 0 && vk < 256);
    auto& cfg = conf::get();
    auto it = std::lower_bound(cfg.binds.begin(), cfg.binds.end(), vk,
                               [](const auto& a, int key) { return a.key < key; });
    bool prev = kbd_state[vk];
    kbd_state[vk] = pressed;
    while (it != cfg.binds.end() && it->key == vk) {
        auto& bind = *it;
        bool matches_mod = true;
        // TODO: improve so keys wont overlap
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
            state::set_key_down(bind.extra, pressed);
            break;
        }
        case conf::Task::Menu: {
            if (pressed)
                cfg.show_menu = !cfg.show_menu;
            break;
        }
        case conf::Task::None: {
            ENSURE(false);
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

void input::sim_key_event(int vk, bool down) {
    if (plug::get().need_key_message)
        MainWindowProcO(::hwnd, down ? WM_KEYDOWN : WM_KEYUP, vk,
                        down ? 0 : ((1 << 30) | (1 << 31)));
}

void input::sim_mouse_event(int vk, bool down) {
    UINT msg;
    switch (vk) {
    case VK_LBUTTON:
        msg = down ? WM_LBUTTONDOWN : WM_LBUTTONUP;
        break;
    case VK_MBUTTON:
        msg = down ? WM_MBUTTONDOWN : WM_MBUTTONUP;
        break;
    case VK_RBUTTON:
        msg = down ? WM_RBUTTONDOWN : WM_RBUTTONUP;
        break;
    default:
        ASS(false);
        return;
    }
    // TODO: implement lParam + wParam if needed
    state::set_key_down(vk, down);
    EditWindowProcO(::mhwnd, msg, vk, down ? 0 : ((1 << 30) | (1 << 31)));
}

void input::sim_mouse_move(int x, int y) {
    // TODO: option in config to send WM_MOUSEMOVE
    if (!plug::get().need_key_message)
        return;
}
