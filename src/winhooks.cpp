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

// FIXME: forced resolution, make it working not only for recording

using ost::string_view;
using std::string;

namespace winhooks {
static bool is_custom_window;
static HHOOK hCbtHook;
} // namespace winhooks

HWND hwnd;
HWND mhwnd;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);
extern bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr);

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

LRESULT(__stdcall* MainWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall MainWindowProcH(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DROPFILES)
        return 0;
    if (!winhooks::is_custom_window || uMsg < WM_MOUSEFIRST || uMsg > WM_MOUSELAST) {
        ui::set_processing(true);
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        ui::set_processing(false);
    }
    switch (uMsg) {
    case WM_KEYDOWN:
    case WM_KEYUP:
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
            DWORD style = GetWindowLongPtr(hWnd, GWL_STYLE);
            DWORD exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
            BOOL hasMenu = (GetMenu(hWnd) != nullptr);
            RECT rect = {0, 0, cfg.forced_res.first, cfg.forced_res.second};
            AdjustWindowRectEx(&rect, style, hasMenu, exStyle);
            mmi->ptMaxTrackSize.x = rect.right - rect.left;
            mmi->ptMaxTrackSize.y = rect.bottom - rect.top;
        }
        return FALSE;
    }
    }
    LRESULT lr = 0;
    if (UAHWndProc(hWnd, uMsg, wParam, lParam, &lr))
        return lr;
    auto ret = MainWindowProcO(hWnd, uMsg, wParam, lParam);
    return ret;
}

LRESULT(__stdcall* EditWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall EditWindowProcH(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DROPFILES)
        return FALSE;
    if (!winhooks::is_custom_window || uMsg < WM_MOUSEFIRST || uMsg > WM_MOUSELAST) {
        ui::set_processing(true);
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        ui::set_processing(false);
    }
    if (uMsg == WM_KEYDOWN || uMsg == WM_KEYUP) {
        // lParam = 0;
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
        conf::get().is_unicode = unicode;
        ::hwnd = hwnd;
        winhooks::fix_win32_theme(hwnd);
        spdlog::info("Mf2MainClassTh window created");
    } else if (class_name == "Mf2EditClassTh") {
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
        winhooks::fix_win32_set_dark_style(hwnd, L"DarkMode");
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
    if (nCode == HCBT_CREATEWND) {
        auto hwnd = reinterpret_cast<HWND>(wParam);
        winhooks::fix_win32_theme_instant(hwnd);

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

static int(WINAPI* MessageBoxAO)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
static int WINAPI MessageBoxAH(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
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

static int(WINAPI* MessageBoxWO)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
static int WINAPI MessageBoxWH(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
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

static BOOL(WINAPI* GetClientRectO)(HWND hWnd, LPRECT lpRect);
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

void winhooks::init() {
    is_custom_window = false;
    hwnd = mhwnd = nullptr;
    // winhooks::init_win32_theme();
    // IAT_STR_ONLY("user32.dll", GetMonitorInfo);
    IAT_STR_AUTO("user32.dll", CreateWindowEx);
    IAT_STR_AUTO("user32.dll", SetWindowText);
    IAT_STR_ONLY("user32.dll", DialogBoxParam);
    IAT_STR_AUTO("user32.dll", SetWindowsHookEx);
    IAT_AUTO("user32.dll", GetClientRect);
    IAT_AUTO("user32.dll", GetFocus);
    IAT_AUTO("user32.dll", GetForegroundWindow);
    IAT_ONLY("user32.dll", SetForegroundWindow);
    IAT_AUTO("user32.dll", GetActiveWindow);
    IAT_ONLY("user32.dll", SetFocus);
    IAT_ONLY("user32.dll", SetActiveWindow);
    IAT_AUTO("user32.dll", GetSystemMetrics);
    IAT_AUTO("user32.dll", SetMenu);
}

void winhooks::after_ui_init() {
    is_custom_window = conf::get().custom_window;
    winhooks::init_win32_theme();
    bool use_w = conf::get().is_unicode;
    ENSURE(hwnd != nullptr);
    ENSURE(mhwnd != nullptr);
    LRESULT lr;
    UAHWndProc(::hwnd, WM_THEMECHANGED, 0, 0, &lr); // Hacky update for dark mode
    MainWindowProcO = reinterpret_cast<WNDPROC>((use_w ? SetWindowLongPtrW : SetWindowLongPtrA)(
        ::hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindowProcH)));
    ENSURE(MainWindowProcO != nullptr);
    EditWindowProcO = reinterpret_cast<WNDPROC>((use_w ? SetWindowLongPtrW : SetWindowLongPtrA)(
        ::mhwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditWindowProcH)));
    ENSURE(EditWindowProcO != nullptr);
    // Let's do it here if the game wants to show an error during startup
    IAT_STR_AUTO("user32.dll", MessageBox);
}

std::pair<int, int> winhooks::get_size() {
    RECT rect;
    if (!GetClientRect(::hwnd, &rect))
        return {0, 0};
    return {static_cast<int>(rect.right - rect.left), static_cast<int>(rect.bottom - rect.top)};
}

void winhooks::display_ensure_fail(ost::string_view text) {
    auto buf = uconv::to_utf16(text);
    ENSURE(buf != nullptr);
    MessageBoxWO(::hwnd, buf, L"Assertion failed!", MB_ICONERROR);
    std::free(buf);
}
