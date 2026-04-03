#define WIN32_LEAN_AND_MEAN
#include "winhooks.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "input.hpp"
#include "mem.hpp"
#include "uconv.hpp"
#include "ui.hpp"
#include <Windows.h>
#include <backends/imgui_impl_win32.h>
#include <spdlog/spdlog.h>

// TODO: hook window title set

using std::string, std::string_view;

HWND hwnd;
HWND mhwnd;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);
extern bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr);

static LRESULT(__stdcall* MainWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall MainWindowProcH(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DROPFILES)
        return 0;
    ui::processing = true;
    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    ui::processing = false;
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
    }
    LRESULT lr = 0;
    if (UAHWndProc(hWnd, uMsg, wParam, lParam, &lr))
        return lr;
    auto ret = MainWindowProcO(hWnd, uMsg, wParam, lParam);
    return ret;
}

static LRESULT(__stdcall* EditWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT __stdcall EditWindowProcH(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_DROPFILES)
        return 0;
    ui::processing = true;
    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    ui::processing = false;
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
    }
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

static BOOL WINAPI SetForegroundWindowH(HWND hWnd) {
    spdlog::info("Failing SetForegroundWindow");
    return FALSE;
}

static HWND(WINAPI* GetActiveWindowO)();
static HWND WINAPI GetActiveWindowH() {
    if (ui::processing)
        return GetActiveWindowO();
    return ::hwnd;
}

static BOOL (WINAPI* SetWindowTextWO)(HWND hWnd, LPCWSTR lpString);
static BOOL WINAPI SetWindowTextWH(HWND hWnd, LPCWSTR lpString) {
    if (hWnd != ::hwnd)
        return SetWindowTextWO(hWnd, lpString);
    string new_title = uconv::from_utf16(lpString) + " [OverFusion]";
    auto temp = uconv::to_utf16(new_title);
    auto ret = SetWindowTextWO(hWnd, temp);
    std::free(temp);
    return ret;
}

static BOOL (WINAPI* SetWindowTextAO)(HWND hWnd, LPCSTR lpString);
static BOOL WINAPI SetWindowTextAH(HWND hWnd, LPCSTR lpString) {
    if (hWnd != ::hwnd)
        return SetWindowTextAO(hWnd, lpString);
    string new_title = uconv::from_ansi(lpString) + " [OverFusion]";
    auto temp = uconv::to_ansi(new_title);
    auto ret = SetWindowTextAO(hWnd, temp);
    std::free(temp);
    return ret;
}

static HHOOK WINAPI SetWindowsHookExAH(int idHook, HOOKPROC lpfn, HINSTANCE hmod,
                                       DWORD dwThreadId) {
    // No size/move hooks which cause desyncs
    spdlog::debug("Failing SetWindowsHookExA");
    return nullptr;
}

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

void winhooks::init() {
    hwnd = mhwnd = nullptr;
    // HOOK_STR_ONLY("user32.dll", GetMonitorInfo);
    HOOK_STR_AUTO("user32.dll", CreateWindowEx);
    HOOK_STR_AUTO("user32.dll", SetWindowText);
    HOOK_STR_ONLY("user32.dll", DialogBoxParam);
    HOOK_STR_ONLY("user32.dll", SetWindowsHookEx);
    HOOK_AUTO("user32.dll", GetFocus);
    HOOK_AUTO("user32.dll", GetForegroundWindow);
    HOOK_ONLY("user32.dll", SetForegroundWindow);
    HOOK_AUTO("user32.dll", GetActiveWindow);
    HOOK_ONLY("user32.dll", SetFocus);
    HOOK_ONLY("user32.dll", SetActiveWindow);
    HOOK_AUTO("user32.dll", GetSystemMetrics);
}

void winhooks::after_ui_init() {
    bool use_w = conf::get().is_unicode;
    ASS(hwnd != nullptr);
    ASS(mhwnd != nullptr);
    LRESULT lr;
    UAHWndProc(::hwnd, WM_THEMECHANGED, 0, 0, &lr); // Hacky update for dark mode
    MainWindowProcO = reinterpret_cast<WNDPROC>((use_w ? SetWindowLongPtrW : SetWindowLongPtrA)(
        ::hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindowProcH)));
    ASS(MainWindowProcO != nullptr);
    EditWindowProcO = reinterpret_cast<WNDPROC>((use_w ? SetWindowLongPtrW : SetWindowLongPtrA)(
        ::mhwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditWindowProcH)));
    ASS(EditWindowProcO != nullptr);
}

void winhooks::sim_key_event(int vk, bool down) {
    MainWindowProcO(::hwnd, down ? WM_KEYDOWN : WM_KEYUP, vk, down ? 0 : ((1 << 30) | (1 << 31)));
}
