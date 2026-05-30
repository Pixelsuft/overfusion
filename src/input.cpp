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

// TODO: maybe reverse it, and use switch-case for backwards?
static const std::map<int, ost::string_view> vk_map = {{VK_F1, "F1"},
                                                       {VK_F2, "F2"},
                                                       {VK_F3, "F3"},
                                                       {VK_F4, "F4"},
                                                       {VK_F5, "F5"},
                                                       {VK_F6, "F6"},
                                                       {VK_F7, "F7"},
                                                       {VK_F8, "F8"},
                                                       {VK_F9, "F9"},
                                                       {VK_F10, "F10"},
                                                       {VK_F11, "F11"},
                                                       {VK_F12, "F12"},
                                                       {VK_F13, "F13"},
                                                       {VK_F14, "F14"},
                                                       {VK_F15, "F15"},
                                                       {VK_F16, "F16"},
                                                       {VK_F17, "F17"},
                                                       {VK_F18, "F18"},
                                                       {VK_F19, "F19"},
                                                       {VK_F20, "F20"},
                                                       {VK_F21, "F21"},
                                                       {VK_F22, "F22"},
                                                       {VK_F23, "F23"},
                                                       {VK_F24, "F24"},

                                                       {'A', "A"},
                                                       {'B', "B"},
                                                       {'C', "C"},
                                                       {'D', "D"},
                                                       {'E', "E"},
                                                       {'F', "F"},
                                                       {'G', "G"},
                                                       {'H', "H"},
                                                       {'I', "I"},
                                                       {'J', "J"},
                                                       {'K', "K"},
                                                       {'L', "L"},
                                                       {'M', "M"},
                                                       {'N', "N"},
                                                       {'O', "O"},
                                                       {'P', "P"},
                                                       {'Q', "Q"},
                                                       {'R', "R"},
                                                       {'S', "S"},
                                                       {'T', "T"},
                                                       {'U', "U"},
                                                       {'V', "V"},
                                                       {'W', "W"},
                                                       {'X', "X"},
                                                       {'Y', "Y"},
                                                       {'Z', "Z"},

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

                                                       {VK_NUMPAD0, "Num0"},
                                                       {VK_NUMPAD1, "Num1"},
                                                       {VK_NUMPAD2, "Num2"},
                                                       {VK_NUMPAD3, "Num3"},
                                                       {VK_NUMPAD4, "Num4"},
                                                       {VK_NUMPAD5, "Num5"},
                                                       {VK_NUMPAD6, "Num6"},
                                                       {VK_NUMPAD7, "Num7"},
                                                       {VK_NUMPAD8, "Num8"},
                                                       {VK_NUMPAD9, "Num9"},
                                                       {VK_MULTIPLY, "Num_Mul"},
                                                       {VK_ADD, "Num_Add"},
                                                       {VK_SEPARATOR, "Num_Sep"},
                                                       {VK_SUBTRACT, "Num_Sub"},
                                                       {VK_DECIMAL, "Num_Dec"},
                                                       {VK_DIVIDE, "Num_Div"},

                                                       {VK_TAB, "Tab"},
                                                       {VK_SPACE, "Space"},
                                                       {VK_ESCAPE, "Esc"},
                                                       {VK_RETURN, "Enter"},
                                                       {VK_BACK, "Backspace"},
                                                       {VK_INSERT, "Insert"},
                                                       {VK_DELETE, "Del"},
                                                       {VK_HOME, "Home"},
                                                       {VK_END, "End"},
                                                       {VK_PRIOR, "PgUp"},
                                                       {VK_NEXT, "PgDn"},
                                                       {VK_PAUSE, "Pause"},
                                                       {VK_SNAPSHOT, "Print"},

                                                       {VK_CONTROL, "Ctrl"},
                                                       {VK_LCONTROL, "LCtrl"},
                                                       {VK_RCONTROL, "RCtrl"},
                                                       {VK_SHIFT, "Shift"},
                                                       {VK_LSHIFT, "LShift"},
                                                       {VK_RSHIFT, "RShift"},
                                                       {VK_MENU, "Alt"},
                                                       {VK_LMENU, "LAlt"},
                                                       {VK_RMENU, "RAlt"},
                                                       {VK_LWIN, "LWin"},
                                                       {VK_RWIN, "RWin"},
                                                       {VK_CAPITAL, "Caps"},

                                                       {VK_UP, "Up"},
                                                       {VK_DOWN, "Down"},
                                                       {VK_LEFT, "Left"},
                                                       {VK_RIGHT, "Right"},

                                                       {VK_LBUTTON, "LButton"},
                                                       {VK_MBUTTON, "MButton"},
                                                       {VK_RBUTTON, "RButton"},

                                                       {VK_OEM_1, "Semicolon"},
                                                       {VK_OEM_PLUS, "Plus"},
                                                       {VK_OEM_COMMA, "Comma"},
                                                       {VK_OEM_MINUS, "Minus"},
                                                       {VK_OEM_PERIOD, "Period"},
                                                       {VK_OEM_2, "Slash"},
                                                       {VK_OEM_3, "Tilde"},
                                                       {VK_OEM_4, "LBracket"},
                                                       {VK_OEM_5, "Backslash"},
                                                       {VK_OEM_6, "RBracket"},
                                                       {VK_OEM_7, "Quote"}};
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
    ASS(lpPoint != nullptr);
    if (ui::is_processing())
        return GetCursorPosO(lpPoint);
    auto mouse_pos = state::get_mouse_pos();
    lpPoint->x = mouse_pos.first;
    lpPoint->y = mouse_pos.second;
    return ClientToScreen(::hwnd, lpPoint);
}

static BOOL WINAPI SetCursorPosH(int X, int Y) {
    spdlog::debug("SetCursorPos({}, {})", X, Y);
    POINT pos;
    pos.x = X;
    pos.y = Y;
    if (ScreenToClient(::hwnd, &pos))
        return state::set_win_mouse_pos(pos.x, pos.y) ? TRUE : FALSE;
    return FALSE;
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

void input::init() {
    std::memset(kbd_state, 0, sizeof(bool) * 256);
    IAT_AUTO("user32.dll", GetKeyState);
    IAT_AUTO("user32.dll", GetAsyncKeyState);
    IAT_AUTO("user32.dll", GetCursorPos);
    IAT_ONLY("user32.dll", GetInputState);
    IAT_AUTO("user32.dll", GetKeyboardLayout);
    IAT_ONLY("user32.dll", GetKeyboardState);
    IAT_ONLY("user32.dll", SetKeyboardState);
    // IAT_ONLY("user32.dll", OpenClipboard);
    IAT_ONLY("user32.dll", IsClipboardFormatAvailable);
    IAT_ONLY("user32.dll", keybd_event);
    IAT_ONLY("user32.dll", mouse_event);
    IAT_ONLY("user32.dll", SendInput);
    IAT_ONLY("user32.dll", SetCursorPos);
}

ost::optional<int> input::vk_from_string(ost::string_view s) {
    auto it = std::find_if(vk_map.begin(), vk_map.end(), [&s](const auto& pair) {
        return pair.second.size() == s.size() &&
               std::equal(pair.second.begin(), pair.second.end(), s.begin(), [](char a, char b) {
                   return std::tolower(static_cast<unsigned char>(a)) ==
                          std::tolower(static_cast<unsigned char>(b));
               });
    });
    if (it == vk_map.end()) {
        spdlog::error("Unknown keycode string: {}", s);
        return {};
    }
    ASS(it->first != 0);
    return it->first;
}

ost::optional<ost::string_view> input::vk_to_string(int vk) {
    auto it = vk_map.find(vk);
    if (it == vk_map.end()) {
        spdlog::error("Unknown keycode value: {}", vk);
        return {};
    }
    return it->second;
}

void input::handle_input(int vk, bool pressed) {
    // Let's process input only during frame update
    // TODO: or maybe add a config variable
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
        case conf::Task::SaveState:
            if (pressed && !prev && !cfg.show_menu)
                state::save_state(bind.extra);
            break;
        case conf::Task::LoadState:
            if (pressed && !prev && !cfg.show_menu)
                state::load_state(bind.extra);
            break;
        case conf::Task::Advance:
            if (pressed && !cfg.show_menu) {
                cfg.need_advance = true;
                cfg.is_paused = true;
            }
            break;
        case conf::Task::Play:
            if (pressed && cfg.show_menu)
                break;
            if (!bind.extra)
                cfg.is_paused = !pressed;
            else if (pressed)
                cfg.is_paused = !cfg.is_paused;
            break;
        case conf::Task::FastForward:
            if (pressed && cfg.show_menu)
                break;
            if (!bind.extra)
                cfg.fast_forward = pressed;
            else if (pressed)
                cfg.fast_forward = !cfg.fast_forward;
            break;
        case conf::Task::Map:
            if (pressed && (prev || cfg.show_menu))
                break;
            state::set_key_down(bind.extra, pressed);
            break;
        case conf::Task::MouseDown:
            if (pressed && !prev && !cfg.show_menu)
                state::add_mouse_toggle(bind.extra);
            break;
        case conf::Task::MouseMove:
            if (pressed && !prev && !cfg.show_menu)
                state::add_mouse_move();
            break;
        case conf::Task::Menu:
            if (pressed)
                cfg.show_menu = !cfg.show_menu;
            break;
#ifndef _DEBUG
        default: {
            ASS(false);
            break;
        }
#endif
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
    // TODO: implement lParam + wParam if needed
    EditWindowProcO(::mhwnd, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
}

std::pair<int, int> input::get_real_mouse_pos() {
    // Get mouse pos relative to the game window
    POINT pt;
    if (GetCursorPosO(&pt) && ScreenToClient(::hwnd, &pt))
        return {pt.x, pt.y};
    spdlog::error("Failed to get mouse pos");
    return {-100, -100};
}
