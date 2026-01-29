#define WIN32_LEAN_AND_MEAN
#include "winhooks.hpp"
#include "ass.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "uconv.hpp"
#include "ui.hpp"
#include <Windows.h>
#include <backends/imgui_impl_win32.h>
#include <spdlog/spdlog.h>

using std::string, std::string_view;

HWND hwnd;
HWND mhwnd;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

static LRESULT(__stdcall* MainWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall MainWindowProcH(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DROPFILES)
        return 0;
    ui::processing = true;
    ImGui_ImplWin32_WndProcHandler(::hwnd, uMsg, wParam, lParam);
    ui::processing = false;
    auto ret = MainWindowProcO(hWnd, uMsg, wParam, lParam);
    return ret;
}

static LRESULT(__stdcall* EditWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall EditWindowProcH(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DROPFILES)
        return 0;
    ui::processing = true;
    ImGui_ImplWin32_WndProcHandler(::mhwnd, uMsg, wParam, lParam);
    ui::processing = false;
    auto ret = EditWindowProcO(hWnd, uMsg, wParam, lParam);
    return ret;
}

static void on_win_create(HWND hwnd, string_view class_name) {
    if (class_name == "Mf2MainClassTh") {
        ::hwnd = hwnd;
        winhooks::fix_win32_theme();
    } else if (class_name == "Mf2EditClassTh") {
        ::mhwnd = hwnd;
    }
}

static HWND(WINAPI* CreateWindowExAO)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU,
                                      HINSTANCE, LPVOID);
static HWND WINAPI CreateWindowExAH(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName,
                                    DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                                    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                                    LPVOID lpParam) {
    if (reinterpret_cast<size_t>(lpClassName) > 0xFFFF) {
        if (strcmp(lpClassName, "mdiclient") == 0) {
            spdlog::warn("Blocking CreateWindowEx");
            return nullptr;
        }
    }
    auto ret = CreateWindowExAO(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth,
                                nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (ret && reinterpret_cast<size_t>(lpClassName) > 0xFFFF) {
        on_win_create(ret, uconv::from_ansi(lpClassName));
    }
    return ret;
}

static HWND(WINAPI* CreateWindowExWO)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND,
                                      HMENU, HINSTANCE, LPVOID);
static HWND WINAPI CreateWindowExWH(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
                                    DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                                    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance,
                                    LPVOID lpParam) {
    if (reinterpret_cast<size_t>(lpClassName) > 0xFFFF) {
        if (wcscmp(lpClassName, L"mdiclient") == 0) {
            spdlog::warn("Blocking CreateWindowEx");
            return nullptr;
        }
    }
    auto ret = CreateWindowExWO(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth,
                                nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (ret && reinterpret_cast<size_t>(lpClassName) > 0xFFFF) {
        on_win_create(ret, uconv::from_utf16(lpClassName));
    }
    return ret;
}

static HWND(WINAPI* GetFocusO)();
static HWND WINAPI GetFocusH() {
    if (ui::processing)
        return GetFocusO();
    return ::hwnd;
}

static HWND(WINAPI* GetForegroundWindowO)();
static HWND WINAPI GetForegroundWindowH() {
    if (ui::processing)
        return GetForegroundWindowO();
    return ::hwnd;
}

static HWND(WINAPI* GetActiveWindowO)();
static HWND WINAPI GetActiveWindowH() {
    if (ui::processing)
        return GetActiveWindowO();
    return ::hwnd;
}

static HHOOK WINAPI SetWindowsHookExAH(int idHook, HOOKPROC lpfn, HINSTANCE hmod,
                                       DWORD dwThreadId) {
    // No size/move hooks which cause desyncs
    return nullptr;
}

static HHOOK WINAPI SetWindowsHookExWH(int idHook, HOOKPROC lpfn, HINSTANCE hmod,
                                       DWORD dwThreadId) {
    return nullptr;
}

static HWND WINAPI SetFocusH(HWND hWnd) { return nullptr; }

static HWND WINAPI SetActiveWindowH(HWND hWnd) { return nullptr; }

static int(WINAPI* GetSystemMetricsO)(int nIndex);
static int WINAPI GetSystemMetricsH(int nIndex) {
    // TODO
    // also SM_XVIRTUALSCREEN, SM_XVIRTUALSCREEN, SM_CXVIRTUALSCREEN, SM_CYVIRTUALSCREEN
    switch (nIndex) {
    case SM_CMONITORS:
        return 1;
    case SM_SAMEDISPLAYFORMAT:
        return 1;
    case SM_CXVSCROLL:
    case SM_CYHSCROLL:
    case SM_CYCAPTION:
    case SM_CYSIZE:
    case SM_CXFRAME:
    case SM_CYFRAME:
    case SM_CYVSCROLL:
    case SM_CXHSCROLL:
        return 0;
    default:
        return GetSystemMetricsO(nIndex);
    }
}

static BOOL WINAPI GetMonitorInfoAH(HMONITOR hMonitor, LPMONITORINFO lpmi) { return FALSE; }

static BOOL WINAPI GetMonitorInfoWH(HMONITOR hMonitor, LPMONITORINFO lpmi) {
    // FIXME
    return FALSE;
}

void winhooks::init() {
    hwnd = mhwnd = nullptr;
    HOOK_AUTO("user32.dll", CreateWindowExW);
    HOOK_AUTO("user32.dll", CreateWindowExA);
    HOOK_AUTO("user32.dll", GetFocus);
    HOOK_AUTO("user32.dll", GetForegroundWindow);
    HOOK_AUTO("user32.dll", GetActiveWindow);
    HOOK_ONLY("user32.dll", SetWindowsHookExA);
    HOOK_ONLY("user32.dll", SetWindowsHookExW);
    HOOK_ONLY("user32.dll", SetFocus);
    HOOK_ONLY("user32.dll", SetActiveWindow);
    HOOK_AUTO("user32.dll", GetSystemMetrics);
    // HOOK_ONLY("user32.dll", GetMonitorInfoA);
    // HOOK_ONLY("user32.dll", GetMonitorInfoW);
}

void winhooks::after_ui_init() {
    bool use_w = plug::get().unicode;
    ASS(hwnd != nullptr);
    ASS(mhwnd != nullptr);
    MainWindowProcO = reinterpret_cast<WNDPROC>((use_w ? SetWindowLongPtrW : SetWindowLongPtrA)(
        ::hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindowProcH)));
    ASS(MainWindowProcO != nullptr);
    EditWindowProcO = reinterpret_cast<WNDPROC>((use_w ? SetWindowLongPtrW : SetWindowLongPtrA)(
        ::mhwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditWindowProcH)));
    ASS(EditWindowProcO != nullptr);
}
