#define WIN32_LEAN_AND_MEAN
#include "input.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "mem.hpp"
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

static const std::map<ost::string_view, int> vk_map = {{"f1", VK_F1},
                                                       {"f2", VK_F2},
                                                       {"f3", VK_F3},
                                                       {"f4", VK_F4},
                                                       {"f5", VK_F5},
                                                       {"f6", VK_F6},
                                                       {"f7", VK_F7},
                                                       {"f8", VK_F8},
                                                       {"f9", VK_F9},
                                                       {"f10", VK_F10},
                                                       {"f11", VK_F11},
                                                       {"f12", VK_F12},
                                                       {"f13", VK_F13},
                                                       {"f14", VK_F14},
                                                       {"f15", VK_F15},
                                                       {"f16", VK_F16},
                                                       {"f17", VK_F17},
                                                       {"f18", VK_F18},
                                                       {"f19", VK_F19},
                                                       {"f20", VK_F20},
                                                       {"f21", VK_F21},
                                                       {"f22", VK_F22},
                                                       {"f23", VK_F23},
                                                       {"f24", VK_F24},

                                                       {"a", 'A'},
                                                       {"b", 'B'},
                                                       {"c", 'C'},
                                                       {"d", 'D'},
                                                       {"e", 'E'},
                                                       {"f", 'F'},
                                                       {"g", 'G'},
                                                       {"h", 'H'},
                                                       {"i", 'I'},
                                                       {"j", 'J'},
                                                       {"k", 'K'},
                                                       {"l", 'L'},
                                                       {"m", 'M'},
                                                       {"n", 'N'},
                                                       {"o", 'O'},
                                                       {"p", 'P'},
                                                       {"q", 'Q'},
                                                       {"r", 'R'},
                                                       {"s", 'S'},
                                                       {"t", 'T'},
                                                       {"u", 'U'},
                                                       {"v", 'V'},
                                                       {"w", 'W'},
                                                       {"x", 'X'},
                                                       {"y", 'Y'},
                                                       {"z", 'Z'},

                                                       {"0", '0'},
                                                       {"1", '1'},
                                                       {"2", '2'},
                                                       {"3", '3'},
                                                       {"4", '4'},
                                                       {"5", '5'},
                                                       {"6", '6'},
                                                       {"7", '7'},
                                                       {"8", '8'},
                                                       {"9", '9'},

                                                       {"num0", VK_NUMPAD0},
                                                       {"num1", VK_NUMPAD1},
                                                       {"num2", VK_NUMPAD2},
                                                       {"num3", VK_NUMPAD3},
                                                       {"num4", VK_NUMPAD4},
                                                       {"num5", VK_NUMPAD5},
                                                       {"num6", VK_NUMPAD6},
                                                       {"num7", VK_NUMPAD7},
                                                       {"num8", VK_NUMPAD8},
                                                       {"num9", VK_NUMPAD9},
                                                       {"num_mul", VK_MULTIPLY},
                                                       {"num_add", VK_ADD},
                                                       {"num_sep", VK_SEPARATOR},
                                                       {"num_sub", VK_SUBTRACT},
                                                       {"num_dec", VK_DECIMAL},
                                                       {"num_div", VK_DIVIDE},

                                                       {"tab", VK_TAB},
                                                       {"space", VK_SPACE},
                                                       {"esc", VK_ESCAPE},
                                                       {"enter", VK_RETURN},
                                                       {"backspace", VK_BACK},
                                                       {"insert", VK_INSERT},
                                                       {"del", VK_DELETE},
                                                       {"home", VK_HOME},
                                                       {"end", VK_END},
                                                       {"pgup", VK_PRIOR},
                                                       {"pgdn", VK_NEXT},
                                                       {"pause", VK_PAUSE},
                                                       {"print", VK_SNAPSHOT},

                                                       {"ctrl", VK_CONTROL},
                                                       {"lctrl", VK_LCONTROL},
                                                       {"rctrl", VK_RCONTROL},
                                                       {"shift", VK_SHIFT},
                                                       {"lshift", VK_LSHIFT},
                                                       {"rshift", VK_RSHIFT},
                                                       {"alt", VK_MENU},
                                                       {"lalt", VK_LMENU},
                                                       {"ralt", VK_RMENU},
                                                       {"lwin", VK_LWIN},
                                                       {"rwin", VK_RWIN},
                                                       {"caps", VK_CAPITAL},

                                                       {"up", VK_UP},
                                                       {"down", VK_DOWN},
                                                       {"left", VK_LEFT},
                                                       {"right", VK_RIGHT},

                                                       {"lbutton", VK_LBUTTON},
                                                       {"mbutton", VK_MBUTTON},
                                                       {"rbutton", VK_RBUTTON},

                                                       {"semicolon", VK_OEM_1},
                                                       {"plus", VK_OEM_PLUS},
                                                       {"comma", VK_OEM_COMMA},
                                                       {"minus", VK_OEM_MINUS},
                                                       {"period", VK_OEM_PERIOD},
                                                       {"slash", VK_OEM_2},
                                                       {"tilde", VK_OEM_3},
                                                       {"lbracket", VK_OEM_4},
                                                       {"backslash", VK_OEM_5},
                                                       {"rbracket", VK_OEM_6},
                                                       {"quote", VK_OEM_7}};
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
    if (conf::get().no_mouse_manipulation)
        return FALSE;
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
    std::string lowered(s);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);
    auto it = vk_map.find(lowered);
    if (it == vk_map.end()) {
        spdlog::error("Unknown keycode string: {}", s);
        return {};
    }
    ASS(it->second != 0);
    return it->second;
}

ost::optional<ost::string_view> input::vk_to_string(int vk) {
    switch (vk) {
    case VK_F1:
        return "F1";
    case VK_F2:
        return "F2";
    case VK_F3:
        return "F3";
    case VK_F4:
        return "F4";
    case VK_F5:
        return "F5";
    case VK_F6:
        return "F6";
    case VK_F7:
        return "F7";
    case VK_F8:
        return "F8";
    case VK_F9:
        return "F9";
    case VK_F10:
        return "F10";
    case VK_F11:
        return "F11";
    case VK_F12:
        return "F12";
    case VK_F13:
        return "F13";
    case VK_F14:
        return "F14";
    case VK_F15:
        return "F15";
    case VK_F16:
        return "F16";
    case VK_F17:
        return "F17";
    case VK_F18:
        return "F18";
    case VK_F19:
        return "F19";
    case VK_F20:
        return "F20";
    case VK_F21:
        return "F21";
    case VK_F22:
        return "F22";
    case VK_F23:
        return "F23";
    case VK_F24:
        return "F24";

    case 'A':
        return "A";
    case 'B':
        return "B";
    case 'C':
        return "C";
    case 'D':
        return "D";
    case 'E':
        return "E";
    case 'F':
        return "F";
    case 'G':
        return "G";
    case 'H':
        return "H";
    case 'I':
        return "I";
    case 'J':
        return "J";
    case 'K':
        return "K";
    case 'L':
        return "L";
    case 'M':
        return "M";
    case 'N':
        return "N";
    case 'O':
        return "O";
    case 'P':
        return "P";
    case 'Q':
        return "Q";
    case 'R':
        return "R";
    case 'S':
        return "S";
    case 'T':
        return "T";
    case 'U':
        return "U";
    case 'V':
        return "V";
    case 'W':
        return "W";
    case 'X':
        return "X";
    case 'Y':
        return "Y";
    case 'Z':
        return "Z";

    case '0':
        return "0";
    case '1':
        return "1";
    case '2':
        return "2";
    case '3':
        return "3";
    case '4':
        return "4";
    case '5':
        return "5";
    case '6':
        return "6";
    case '7':
        return "7";
    case '8':
        return "8";
    case '9':
        return "9";

    case VK_NUMPAD0:
        return "Num0";
    case VK_NUMPAD1:
        return "Num1";
    case VK_NUMPAD2:
        return "Num2";
    case VK_NUMPAD3:
        return "Num3";
    case VK_NUMPAD4:
        return "Num4";
    case VK_NUMPAD5:
        return "Num5";
    case VK_NUMPAD6:
        return "Num6";
    case VK_NUMPAD7:
        return "Num7";
    case VK_NUMPAD8:
        return "Num8";
    case VK_NUMPAD9:
        return "Num9";
    case VK_MULTIPLY:
        return "Num_Mul";
    case VK_ADD:
        return "Num_Add";
    case VK_SEPARATOR:
        return "Num_Sep";
    case VK_SUBTRACT:
        return "Num_Sub";
    case VK_DECIMAL:
        return "Num_Dec";
    case VK_DIVIDE:
        return "Num_Div";

    case VK_TAB:
        return "Tab";
    case VK_SPACE:
        return "Space";
    case VK_ESCAPE:
        return "Esc";
    case VK_RETURN:
        return "Enter";
    case VK_BACK:
        return "Backspace";
    case VK_INSERT:
        return "Insert";
    case VK_DELETE:
        return "Del";
    case VK_HOME:
        return "Home";
    case VK_END:
        return "End";
    case VK_PRIOR:
        return "PgUp";
    case VK_NEXT:
        return "PgDn";
    case VK_PAUSE:
        return "Pause";
    case VK_SNAPSHOT:
        return "Print";

    case VK_CONTROL:
        return "Ctrl";
    case VK_LCONTROL:
        return "LCtrl";
    case VK_RCONTROL:
        return "RCtrl";
    case VK_SHIFT:
        return "Shift";
    case VK_LSHIFT:
        return "LShift";
    case VK_RSHIFT:
        return "RShift";
    case VK_MENU:
        return "Alt";
    case VK_LMENU:
        return "LAlt";
    case VK_RMENU:
        return "RAlt";
    case VK_LWIN:
        return "LWin";
    case VK_RWIN:
        return "RWin";
    case VK_CAPITAL:
        return "Caps";

    case VK_UP:
        return "Up";
    case VK_DOWN:
        return "Down";
    case VK_LEFT:
        return "Left";
    case VK_RIGHT:
        return "Right";

    case VK_LBUTTON:
        return "LButton";
    case VK_MBUTTON:
        return "MButton";
    case VK_RBUTTON:
        return "RButton";

    case VK_OEM_1:
        return "Semicolon";
    case VK_OEM_PLUS:
        return "Plus";
    case VK_OEM_COMMA:
        return "Comma";
    case VK_OEM_MINUS:
        return "Minus";
    case VK_OEM_PERIOD:
        return "Period";
    case VK_OEM_2:
        return "Slash";
    case VK_OEM_3:
        return "Tilde";
    case VK_OEM_4:
        return "LBracket";
    case VK_OEM_5:
        return "Backslash";
    case VK_OEM_6:
        return "RBracket";
    case VK_OEM_7:
        return "Quote";

    default:
        // spdlog::error("Unknown keycode value: {}", vk);
        return {};
    }
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
    for (int i = 0; i < 256; i++) {
        if (kbd_state[i] && !(GetKeyStateO(i) & 0x8000)) {
            // Key up event may not be sent automatically when messagebox appears
            handle_input_real(i, false);
        }
    }
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
    if (conf::get().need_key_message)
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
    // TODO: implement wParam if needed
    if (conf::get().need_mouse_move_message)
        EditWindowProcO(::mhwnd, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
}

std::pair<int, int> input::get_real_mouse_pos() {
    // Get mouse pos relative to the game window
    POINT pt;
    if (GetCursorPosO(&pt) && ScreenToClient(::mhwnd, &pt))
        return {pt.x, pt.y};
    spdlog::error("Failed to get mouse pos");
    return {-100, -100};
}
