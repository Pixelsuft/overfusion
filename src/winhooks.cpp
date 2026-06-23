#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "winhooks.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "input.hpp"
#include "mem.hpp"
#include "state.hpp"
#include "uconv.hpp"
#include "ui.hpp"
#include <Windows.h>
#include <backends/imgui_impl_win32.h>
#include <spdlog/spdlog.h>

using ost::string_view;
using std::string;

namespace winhooks {
static HHOOK hCbtHook;
static bool inited;
} // namespace winhooks

HWND hwnd;
HWND mhwnd;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);
extern bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr);

static std::pair<int, int> get_needed_win_size(HWND hWnd, int w, int h) {
    DWORD style = GetWindowLongPtr(hWnd, GWL_STYLE);
    DWORD exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
    BOOL hasMenu = (GetMenu(hWnd) != nullptr);
    RECT rect = {0, 0, w, h};
    AdjustWindowRectEx(&rect, style, hasMenu, exStyle);
    return {rect.right - rect.left, rect.bottom - rect.top};
}

static int(WINAPI* GetSystemMetricsO)(int nIndex);
static int WINAPI GetSystemMetricsH(int nIndex) {
    // TODO
    // also SM_XVIRTUALSCREEN, SM_XVIRTUALSCREEN, SM_CXVIRTUALSCREEN, SM_CYVIRTUALSCREEN
    switch (nIndex) {
    case SM_CMONITORS:
        return 1;
    case SM_SAMEDISPLAYFORMAT:
        return 1;
    case SM_CYMENU:
        return conf::get().disable_app_menu ? 0 : GetSystemMetricsO(SM_CYMENU);
    case SM_CXVSCROLL:
    case SM_CYHSCROLL:
    case SM_CYCAPTION:
    case SM_CYSIZE:
    case SM_CXFRAME:
    case SM_CYFRAME:
    case SM_CYVSCROLL:
    case SM_CXHSCROLL:
    default:
        return GetSystemMetricsO(nIndex);
    }
}

LRESULT(WINAPI* MainWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI MainWindowProcH(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DROPFILES)
        return 0;
    if (winhooks::inited && !conf::get().custom_window || uMsg < WM_MOUSEFIRST ||
        uMsg > WM_MOUSELAST) {
        ui::set_processing(true);
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        ui::set_processing(false);
    }
    switch (uMsg) {
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (winhooks::inited)
            input::handle_input(wParam, uMsg == WM_KEYDOWN);
        return FALSE;
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_MOUSEHWHEEL:
    case WM_CHAR:
        return FALSE;
    case WM_GETMINMAXINFO: {
        auto& cfg = conf::get();
        if (cfg.forced_res.first > 0 && cfg.forced_res.second > 0) {
            auto mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            auto needed_size =
                get_needed_win_size(hWnd, cfg.forced_res.first, cfg.forced_res.second);
            mmi->ptMaxTrackSize.x = needed_size.first;
            mmi->ptMaxTrackSize.y = needed_size.second;
            return FALSE;
        }
    }
    case WM_PAINT:
        if (!conf::get().disable_dark_mode_support)
            winhooks::fix_win32_window_bg(hWnd);
        break;
    case WM_ERASEBKGND:
        if (!conf::get().disable_dark_mode_support && winhooks::fix_win32_window_bg(hWnd))
            return TRUE;
        break;
    }
    LRESULT lr = 0;
    if (UAHWndProc(hWnd, uMsg, wParam, lParam, &lr))
        return lr;
    auto ret = MainWindowProcO(hWnd, uMsg, wParam, lParam);
    return ret;
}

LRESULT(WINAPI* EditWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI EditWindowProcH(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DROPFILES)
        return FALSE;
    if (winhooks::inited && !conf::get().custom_window || uMsg < WM_MOUSEFIRST ||
        uMsg > WM_MOUSELAST) {
        ui::set_processing(true);
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        ui::set_processing(false);
    }
    if (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP) {
        // lParam = 0;
        if (winhooks::inited)
            input::handle_input(wParam, uMsg == WM_KEYDOWN);
        return FALSE;
    }
    if (uMsg == WM_MOUSELEAVE || uMsg == WM_MOUSEHWHEEL || uMsg == WM_CHAR)
        return FALSE;
    auto ret = EditWindowProcO(hWnd, uMsg, wParam, lParam);
    return ret;
}

static void on_win_create(HWND hwnd, string_view class_name, bool unicode) {
    if (class_name == "Mf2MainClassTh") {
        ENSURE(MainWindowProcO != nullptr);
        ::hwnd = hwnd;
        winhooks::init_win32_theme();
        winhooks::fix_win32_theme(hwnd);
        LRESULT lr;
        UAHWndProc(::hwnd, WM_THEMECHANGED, 0, 0, &lr); // Hacky update for dark mode menu
        spdlog::info("Mf2MainClassTh window created");
    } else if (class_name == "Mf2EditClassTh") {
        ENSURE(EditWindowProcO != nullptr);
        ::mhwnd = hwnd;
        spdlog::info("Mf2EditClassTh window created");
    } else if (class_name == "SysListView32" || class_name == "SysHeader32")
        winhooks::fix_win32_set_dark_style(hwnd, L"DarkMode_ItemsView");
    else if (class_name == "COMBOBOX")
        winhooks::fix_win32_set_dark_style(hwnd, L"DarkMode_CFD");
    else if (class_name == "EDIT" || class_name == "BUTTON" || class_name == "LISTBOX" ||
             class_name == "SCROLLBAR" || class_name == "TOOLTIPS_CLASS" ||
             class_name == "msctls_trackbar32" || class_name == "msctls_progress32" ||
             class_name == "SysTabControl32")
        winhooks::fix_win32_set_dark_style(hwnd, L"DarkMode_Explorer");
    else if (class_name == "msctls_statusbar32")
        winhooks::fix_win32_set_dark_style(hwnd, L"ExplorerStatusBar");
    else if (class_name == "RebarWindow32")
        winhooks::fix_win32_set_dark_style(hwnd, L"DarkModeNavbar");
}

static HWND(WINAPI* CreateWindowExAO)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU,
                                      HINSTANCE, LPVOID);
static HWND WINAPI CreateWindowExAH(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
                                    DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                                    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                                    LPVOID lpParam) {
    auto ret = CreateWindowExAO(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth,
                                nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (ret && reinterpret_cast<size_t>(lpClassName) > 0xFFFF) {
        on_win_create(ret, uconv::from_ansi(lpClassName), false);
        winhooks::fix_win32_theme_instant(ret);
    }
    return ret;
}

static HWND(WINAPI* CreateWindowExWO)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND,
                                      HMENU, HINSTANCE, LPVOID);
static HWND WINAPI CreateWindowExWH(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
                                    DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                                    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                                    LPVOID lpParam) {
    auto ret = CreateWindowExWO(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth,
                                nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (ret && reinterpret_cast<size_t>(lpClassName) > 0xFFFF) {
        on_win_create(ret, uconv::from_utf16(lpClassName), true);
        winhooks::fix_win32_theme_instant(ret);
    }
    return ret;
}

static void on_class_create_ansi(const char* name, WNDPROC& proc) {
    ASS(name != nullptr);
    // We all know it's editable :)
    if (!MainWindowProcO && strcmp(name, "Mf2MainClassTh") == 0) {
        MainWindowProcO = proc;
        proc = MainWindowProcH;
    } else if (!EditWindowProcO && strcmp(name, "Mf2EditClassTh") == 0) {
        EditWindowProcO = proc;
        proc = EditWindowProcH;
    }
}

static void on_class_create_wide(const wchar_t* name, WNDPROC& proc) {
    ASS(name != nullptr);
    if (!MainWindowProcO && wcscmp(name, L"Mf2MainClassTh") == 0) {
        MainWindowProcO = proc;
        proc = MainWindowProcH;
    } else if (!EditWindowProcO && wcscmp(name, L"Mf2EditClassTh") == 0) {
        EditWindowProcO = proc;
        proc = EditWindowProcH;
    }
}

static ATOM(WINAPI* RegisterClassAO)(WNDCLASSA* cls);
static ATOM WINAPI RegisterClassAH(WNDCLASSA* cls) {
    on_class_create_ansi(cls->lpszClassName, cls->lpfnWndProc);
    return RegisterClassAO(cls);
}

static ATOM(WINAPI* RegisterClassExAO)(WNDCLASSEXA* cls);
static ATOM WINAPI RegisterClassExAH(WNDCLASSEXA* cls) {
    on_class_create_ansi(cls->lpszClassName, cls->lpfnWndProc);
    if (strcmp(cls->lpszClassName, "Mf2MainClassTh") == 0)
        cls->hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    return RegisterClassExAO(cls);
}

static ATOM(WINAPI* RegisterClassWO)(WNDCLASSW* cls);
static ATOM WINAPI RegisterClassWH(WNDCLASSW* cls) {
    on_class_create_wide(cls->lpszClassName, cls->lpfnWndProc);
    return RegisterClassWO(cls);
}

static ATOM(WINAPI* RegisterClassExWO)(WNDCLASSEXW* cls);
static ATOM WINAPI RegisterClassExWH(WNDCLASSEXW* cls) {
    on_class_create_wide(cls->lpszClassName, cls->lpfnWndProc);
    return RegisterClassExWO(cls);
}

static HWND(WINAPI* GetFocusO)();
static HWND WINAPI GetFocusH() {
    if (ui::is_processing())
        return GetFocusO();
    return ::hwnd;
}

static HWND(WINAPI* GetForegroundWindowO)();
static HWND WINAPI GetForegroundWindowH() {
    if (ui::is_processing())
        return GetForegroundWindowO();
    return ::hwnd;
}

static BOOL WINAPI SetForegroundWindowH(HWND hWnd) {
    spdlog::info("Failing SetForegroundWindow");
    return FALSE;
}

static HWND(WINAPI* GetActiveWindowO)();
static HWND WINAPI GetActiveWindowH() {
    if (ui::is_processing())
        return GetActiveWindowO();
    return ::hwnd;
}

static BOOL(WINAPI* SetWindowTextWO)(HWND hWnd, LPCWSTR lpString);
static BOOL WINAPI SetWindowTextWH(HWND hWnd, LPCWSTR lpString) {
    if (hWnd != ::hwnd)
        return SetWindowTextWO(hWnd, lpString);
    ASS(lpString && std::wcslen(lpString) < (4096 - 13));
    wchar_t temp_buf[4096];
    std::wcscpy(temp_buf, lpString);
    std::wcscat(temp_buf, L" [OverFusion]");
    return SetWindowTextWO(hWnd, temp_buf);
}

static BOOL(WINAPI* SetWindowTextAO)(HWND hWnd, LPCSTR lpString);
static BOOL WINAPI SetWindowTextAH(HWND hWnd, LPCSTR lpString) {
    if (hWnd != ::hwnd)
        return SetWindowTextAO(hWnd, lpString);
    ASS(lpString && std::strlen(lpString) < (4096 - 13));
    char temp_buf[4096];
    std::strcpy(temp_buf, lpString);
    std::strcat(temp_buf, " [OverFusion]");
    return SetWindowTextAO(hWnd, temp_buf);
}

static HHOOK(WINAPI* SetWindowsHookExAO)(int idHook, HOOKPROC lpfn, HINSTANCE hmod,
                                         DWORD dwThreadId);
static HHOOK WINAPI SetWindowsHookExAH(int idHook, HOOKPROC lpfn, HINSTANCE hmod,
                                       DWORD dwThreadId) {
    // No size/move hooks which cause desyncs
    spdlog::debug("Failing SetWindowsHookExA");
    return nullptr;
}

static HHOOK(WINAPI* SetWindowsHookExWO)(int idHook, HOOKPROC lpfn, HINSTANCE hmod,
                                         DWORD dwThreadId);
static HHOOK WINAPI SetWindowsHookExWH(int idHook, HOOKPROC lpfn, HINSTANCE hmod,
                                       DWORD dwThreadId) {
    spdlog::debug("Failing SetWindowsHookExW");
    return nullptr;
}

static HWND WINAPI SetFocusH(HWND hWnd) {
    // Note: it's breaks win32 controls, we don't support them anyway
    spdlog::info("Failing SetFocus");
    return nullptr;
}

static HWND WINAPI SetActiveWindowH(HWND hWnd) {
    spdlog::info("Failing SetActiveWindow");
    return nullptr;
}

static BOOL WINAPI GetMonitorInfoAH(HMONITOR hMonitor, LPMONITORINFO lpmi) { return FALSE; }

static BOOL WINAPI GetMonitorInfoWH(HMONITOR hMonitor, LPMONITORINFO lpmi) {
    // FIXME
    return FALSE;
}

static INT_PTR WINAPI DialogBoxParamAH(HINSTANCE hInstance, LPCSTR lpTemplateName, HWND hWndParent,
                                       DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    spdlog::warn("Failing DialogBoxParamA");
    return static_cast<INT_PTR>(-1);
}

static INT_PTR WINAPI DialogBoxParamWH(HINSTANCE hInstance, LPCWSTR lpTemplateName, HWND hWndParent,
                                       DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    spdlog::warn("Failing DialogBoxParamW");
    return static_cast<INT_PTR>(-1);
}

static BOOL(WINAPI* SetMenuO)(HWND hWnd, HMENU hMenu);
static BOOL WINAPI SetMenuH(HWND hWnd, HMENU hMenu) {
    if (conf::get().disable_app_menu && hWnd == ::hwnd) {
        spdlog::info("Failing to set menu");
        return FALSE;
    }
    return SetMenuO(hWnd, hMenu);
}

static LRESULT CALLBACK CbtDarkHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_CREATEWND && !conf::get().disable_dark_mode_support) {
        auto hwnd = reinterpret_cast<HWND>(wParam);
        // winhooks::fix_win32_theme_instant(hwnd);

        wchar_t className[8];
        if (GetClassNameW(hwnd, className, 8) == 6) {
            if (wcscmp(className, L"#32770") == 0)
                winhooks::fix_win32_theme_messagebox(hwnd);
            else if (wcscmp(className, L"Button") == 0)
                winhooks::fix_win32_set_dark_style(hwnd, L"DarkMode_Explorer");
        }
    }
    return CallNextHookEx(winhooks::hCbtHook, nCode, wParam, lParam);
}

static int(WINAPI* MessageBoxAO)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption,
                                 UINT uType) = MessageBoxA;
int WINAPI MessageBoxAH(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
    ASS(lpText && lpCaption);
    int ret = 0;
    if (!ui::is_processing() && std::strcmp(lpCaption, "Microsoft Visual C++ Runtime Library") != 0)
        ret = state::process_message_box(uconv::from_ansi(lpText), uconv::from_ansi(lpCaption),
                                         uType);
    if (ret == 0) {
        winhooks::hCbtHook =
            SetWindowsHookExWO(WH_CBT, CbtDarkHookProc, nullptr, GetCurrentThreadId());
        ret = MessageBoxAO(hWnd, lpText, lpCaption, uType);
        if (winhooks::hCbtHook)
            UnhookWindowsHookEx(winhooks::hCbtHook);
        state::remember_message_box(ret);
    }
    return ret;
}

static int(WINAPI* MessageBoxWO)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption,
                                 UINT uType) = MessageBoxW;
int WINAPI MessageBoxWH(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    ASS(lpText && lpCaption);
    int ret = 0;
    if (!ui::is_processing() &&
        std::wcscmp(lpCaption, L"Microsoft Visual C++ Runtime Library") != 0)
        ret = state::process_message_box(uconv::from_utf16(lpText), uconv::from_utf16(lpCaption),
                                         uType);
    if (ret == 0) {
        winhooks::hCbtHook =
            SetWindowsHookExWO(WH_CBT, CbtDarkHookProc, nullptr, GetCurrentThreadId());
        ret = MessageBoxWO(hWnd, lpText, lpCaption, uType);
        if (winhooks::hCbtHook)
            UnhookWindowsHookEx(winhooks::hCbtHook);
        state::remember_message_box(ret);
    }
    return ret;
}

BOOL(WINAPI* GetClientRectO)(HWND hWnd, LPRECT lpRect);
static BOOL WINAPI GetClientRectH(HWND hWnd, LPRECT lpRect) {
    if (hWnd == ::hwnd) {
        auto& cfg = conf::get();
        if (cfg.forced_res.first > 0 && cfg.forced_res.second > 0) {
            lpRect->left = 0;
            lpRect->top = 0;
            lpRect->right = cfg.forced_res.first;
            lpRect->bottom = cfg.forced_res.second;
            return TRUE;
        }
    }
    return GetClientRectO(hWnd, lpRect);
}

static DWORD(WINAPI* MsgWaitForMultipleObjectsO)(DWORD nCount, const HANDLE* pHandles,
                                                 BOOL fWaitAll, DWORD dwMilliseconds,
                                                 DWORD dwWakeMask);
static DWORD WINAPI MsgWaitForMultipleObjectsH(DWORD nCount, const HANDLE* pHandles, BOOL fWaitAll,
                                               DWORD dwMilliseconds, DWORD dwWakeMask) {
    spdlog::warn("MsgWaitForMultipleObjects was not patched: {} {} {} {}", nCount, fWaitAll,
                 dwMilliseconds, dwWakeMask);
    return MsgWaitForMultipleObjectsO(nCount, pHandles, fWaitAll, dwMilliseconds, dwWakeMask);
}

static BOOL(WINAPI* SetWindowPosO)(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy,
                                   UINT uFlags);
static BOOL WINAPI SetWindowPosH(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy,
                                 UINT uFlags) {
    if (hWnd == ::hwnd || hWnd == ::mhwnd) {
        auto& cfg = conf::get();
        if (cfg.forced_res.first > 0 && cfg.forced_res.second > 0) {
            if (hWnd == ::hwnd && cfg.forced_window_resize) {
                auto size = get_needed_win_size(hWnd, cfg.forced_res.first, cfg.forced_res.second);
                cx = size.first;
                cy = size.second;
            }
            if (hWnd == ::mhwnd) {
                cx = cfg.forced_res.first;
                cy = cfg.forced_res.second;
            }
        }
        spdlog::debug("SetWindowPos {}: {} {} {} {} {}", hWnd == ::hwnd ? "hwnd" : "mhwnd", X, Y,
                      cx, cy, uFlags);
        // return FALSE;
    }
    return SetWindowPosO(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

static BOOL(WINAPI* MoveWindowO)(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
static BOOL WINAPI MoveWindowH(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint) {
    if (hWnd == ::hwnd || hWnd == ::mhwnd) {
        spdlog::debug("MoveWindow: {} {} {} {} {}", X, Y, nWidth, nHeight, bRepaint);
        return FALSE;
    }
    return MoveWindowO(hWnd, X, Y, nWidth, nHeight, bRepaint);
}

static HDWP(WINAPI* DeferWindowPosO)(HDWP hWinPosInfo, HWND hWnd, HWND hWndInsertAfter, int x,
                                     int y, int cx, int cy, UINT uFlags);
static HDWP WINAPI DeferWindowPosH(HDWP hWinPosInfo, HWND hWnd, HWND hWndInsertAfter, int x, int y,
                                   int cx, int cy, UINT uFlags) {
    return FALSE;
    if (hWnd == ::hwnd || hWnd == ::mhwnd) {
        spdlog::debug("DeferWindowPos: {} {} {} {} {}", x, y, cx, cy, uFlags);
        return FALSE;
    }
    return DeferWindowPosO(hWinPosInfo, hWnd, hWndInsertAfter, x, y, cx, cy, uFlags);
}

static BOOL(WINAPI* SetWindowPlacementO)(HWND hWnd, const WINDOWPLACEMENT* lpwndpl);
static BOOL WINAPI SetWindowPlacementH(HWND hWnd, const WINDOWPLACEMENT* lpwndpl) {
    if (hWnd == ::hwnd || hWnd == ::mhwnd) {
        spdlog::debug("SetWindowPlacement");
        return FALSE;
    }
    return SetWindowPlacementO(hWnd, lpwndpl);
}

static LONG(WINAPI* SetWindowLongAO)(HWND hWnd, int nIndex, LONG dwNewLong);
static LONG WINAPI SetWindowLongAH(HWND hWnd, int nIndex, LONG dwNewLong) {
    if (hWnd == ::hwnd && nIndex == GWL_STYLE) {
        spdlog::debug("SetWindowLongA: {} {}", nIndex, dwNewLong);
        if (conf::get().disable_fullscreen) {
            dwNewLong |= WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_POPUP | WS_MAXIMIZEBOX |
                         WS_MINIMIZEBOX | WS_THICKFRAME;
            return 0;
        }
    }
    return SetWindowLongAO(hWnd, nIndex, dwNewLong);
}

static LONG(WINAPI* SetWindowLongWO)(HWND hWnd, int nIndex, LONG dwNewLong);
static LONG WINAPI SetWindowLongWH(HWND hWnd, int nIndex, LONG dwNewLong) {
    if (hWnd == ::hwnd && nIndex == GWL_STYLE) {
        spdlog::debug("SetWindowLongW: {} {}", nIndex, dwNewLong);
        if (conf::get().disable_fullscreen) {
            dwNewLong |= WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_POPUP | WS_MAXIMIZEBOX |
                         WS_MINIMIZEBOX | WS_THICKFRAME;
            return 0;
        }
    }
    return SetWindowLongWO(hWnd, nIndex, dwNewLong);
}

static BOOL(WINAPI* ShowWindowO)(HWND hWnd, int nCmdShow);
static BOOL WINAPI ShowWindowH(HWND hWnd, int nCmdShow) {
    if (hWnd == ::hwnd) {
        spdlog::debug("ShowWindow {}", nCmdShow);
        if (conf::get().disable_fullscreen) {
            nCmdShow &= ~SW_SHOWMAXIMIZED;
            nCmdShow |= SW_SHOWDEFAULT;
        }
    }
    return ShowWindowO(hWnd, nCmdShow);
}

void winhooks::init() {
    hwnd = mhwnd = nullptr;
    MainWindowProcO = EditWindowProcO = nullptr;
    inited = false;
    winhooks::pre_init_win32_theme();
    // IAT_STR_ONLY("user32.dll", GetMonitorInfo);
    IAT_STR_AUTO("user32.dll", CreateWindowEx);
    IAT_STR_AUTO("user32.dll", RegisterClassEx);
    IAT_STR_AUTO("user32.dll", RegisterClass);
    IAT_STR_AUTO("user32.dll", SetWindowText);
    IAT_STR_ONLY("user32.dll", DialogBoxParam);
    IAT_STR_AUTO("user32.dll", SetWindowsHookEx);
    IAT_STR_AUTO("user32.dll", SetWindowLong);
    IAT_AUTO("user32.dll", GetClientRect);
    IAT_AUTO("user32.dll", GetFocus);
    IAT_AUTO("user32.dll", GetForegroundWindow);
    IAT_ONLY("user32.dll", SetForegroundWindow);
    IAT_AUTO("user32.dll", GetActiveWindow);
    IAT_ONLY("user32.dll", SetFocus);
    IAT_ONLY("user32.dll", SetActiveWindow);
    IAT_AUTO("user32.dll", GetSystemMetrics);
    IAT_AUTO("user32.dll", SetMenu);
    IAT_AUTO("user32.dll", MsgWaitForMultipleObjects);
    IAT_AUTO("user32.dll", SetWindowPos);
    IAT_AUTO("user32.dll", MoveWindow);
    IAT_AUTO("user32.dll", DeferWindowPos);
    IAT_AUTO("user32.dll", SetWindowPlacement);
    IAT_AUTO("user32.dll", ShowWindow);
}

void winhooks::after_ui_init() {
    inited = true;
    ENSURE(hwnd != nullptr);
    ENSURE(mhwnd != nullptr);
    // Let's do it here if the game wants to show an error during startup
#ifdef _DEBUG
    // Default hook for MSVC message boxes
    HOOK_STR_AUTO("user32.dll", MessageBox);
#else
    IAT_STR_AUTO("user32.dll", MessageBox);
#endif
}

std::pair<int, int> winhooks::get_client_size() {
    RECT rect;
    if (!GetClientRectO(::mhwnd, &rect))
        return {0, 0};
    return {static_cast<int>(rect.right - rect.left), static_cast<int>(rect.bottom - rect.top)};
}

void winhooks::display_ensure_fail(const void* text) {
    auto real_text = reinterpret_cast<const wchar_t*>(text);
    if (::hwnd != nullptr) {
        // Dark mode
        auto prev_proc = ui::is_processing();
        auto& cfg = conf::get();
        auto prev_repl = cfg.is_replay;
        ui::set_processing(true);
        cfg.is_replay = true;
        MessageBoxWH(::hwnd, real_text, L"Assertion failed!", MB_ICONERROR);
        cfg.is_replay = prev_repl;
        ui::set_processing(prev_proc);
    } else {
        MessageBoxWO(nullptr, real_text, L"Assertion failed!", MB_ICONERROR);
    }
}
