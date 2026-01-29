#define WIN32_LEAN_AND_MEAN
#include "extrahooks.hpp"
#include "mem.hpp"
#include "state.hpp"
#include "uconv.hpp"
#include "ui.hpp"
#include <WinSock2.h>
#include <Windows.h>
#include <joystickapi.h>
#include <shellapi.h>
#include <spdlog/spdlog.h>

static void(WINAPI* DragAcceptFilesO)(HWND hWnd, BOOL fAccept);
static void WINAPI DragAcceptFilesH(HWND hWnd, BOOL fAccept) { DragAcceptFilesO(hWnd, FALSE); }

static UINT DragQueryFileWH(HDROP hDrop, UINT iFile, LPWSTR lpszFile, UINT cch) { return 0; }
static UINT DragQueryFileAH(HDROP hDrop, UINT iFile, LPSTR lpszFile, UINT cch) { return 0; }

static HINSTANCE ShellExecuteAH(HWND hwnd, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir,
                                INT show) {
    spdlog::info("ShellExecuteA: {} \"{}\"", uconv::from_ansi(op), uconv::from_ansi(file));
    return reinterpret_cast<HINSTANCE>(SE_ERR_ACCESSDENIED);
}

static HINSTANCE ShellExecuteWH(HWND hwnd, LPCWSTR op, LPCWSTR file, LPCWSTR params, LPCWSTR dir,
                                INT show) {
    spdlog::info("ShellExecuteW: {} \"{}\"", uconv::from_utf16(op), uconv::from_utf16(file));
    return reinterpret_cast<HINSTANCE>(SE_ERR_ACCESSDENIED);
}

static BOOL ShellExecuteExAH(SHELLEXECUTEINFOA* pExecInfo) {
    spdlog::info("ShellExecuteExA: {} \"{}\"", uconv::from_ansi(pExecInfo->lpVerb),
                 uconv::from_ansi(pExecInfo->lpFile));
    return FALSE;
}

static BOOL ShellExecuteExWH(SHELLEXECUTEINFOW* pExecInfo) {
    spdlog::info("ShellExecuteExW: {} \"{}\"", uconv::from_utf16(pExecInfo->lpVerb),
                 uconv::from_utf16(pExecInfo->lpFile));
    return FALSE;
}

static int(WINAPI* MessageBoxAO)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
static int WINAPI MessageBoxAH(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    if (ui::processing)
        return MessageBoxAO(hWnd, lpText, lpCaption, uType);
    auto ret = MessageBoxAO(hWnd, lpText, lpCaption, uType);
    if (uType == 0x30)
        state::invalidate_process();
    return ret;
}

static int(WINAPI* MessageBoxWO)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
static int WINAPI MessageBoxWH(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    if (ui::processing)
        return MessageBoxWO(hWnd, lpText, lpCaption, uType);
    auto ret = MessageBoxWO(hWnd, lpText, lpCaption, uType);
    if (uType == 0x30)
        state::invalidate_process();
    return ret;
}

static BOOL WINAPI EnumSystemLocalesAH(LOCALE_ENUMPROCA lpLocaleEnumProc, DWORD dwFlags) {
    char loc[] = "00000409";
    lpLocaleEnumProc(loc);
    return TRUE;
}

static BOOL WINAPI EnumSystemLocalesWH(LOCALE_ENUMPROCW lpLocaleEnumProc, DWORD dwFlags) {
    wchar_t loc[] = L"00000409";
    lpLocaleEnumProc(loc);
    return TRUE;
}

static LCID WINAPI GetUserDefaultLCIDH() { return 0x1000; }

static DWORD WINAPI GetVersionH() { return 0x1DB10106; }

static BOOL WINAPI GetVersionExAH(LPOSVERSIONINFOA lpVersionInformation) {
    if (lpVersionInformation == NULL)
        return FALSE;
    lpVersionInformation->dwMajorVersion = 6;
    lpVersionInformation->dwMinorVersion = 1;
    lpVersionInformation->dwBuildNumber = 7601;
    lpVersionInformation->dwPlatformId = VER_PLATFORM_WIN32_NT;
    if (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXA)) {
        LPOSVERSIONINFOEXA lpEx = reinterpret_cast<LPOSVERSIONINFOEXA>(lpVersionInformation);
        lpEx->wServicePackMajor = 1;
        lpEx->wServicePackMinor = 0;
        lpEx->wSuiteMask = VER_SUITE_SINGLEUSERTS;
        lpEx->wProductType = VER_NT_WORKSTATION;
    }
    return TRUE;
}

static BOOL WINAPI GetVersionExWH(LPOSVERSIONINFOW lpVersionInformation) {
    if (lpVersionInformation == NULL)
        return FALSE;
    lpVersionInformation->dwMajorVersion = 6;
    lpVersionInformation->dwMinorVersion = 1;
    lpVersionInformation->dwBuildNumber = 7601;
    lpVersionInformation->dwPlatformId = VER_PLATFORM_WIN32_NT;
    if (lpVersionInformation->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXW)) {
        LPOSVERSIONINFOEXW lpEx = reinterpret_cast<LPOSVERSIONINFOEXW>(lpVersionInformation);
        lpEx->wServicePackMajor = 1;
        lpEx->wServicePackMinor = 0;
        lpEx->wSuiteMask = VER_SUITE_SINGLEUSERTS;
        lpEx->wProductType = VER_NT_WORKSTATION;
    }
    return TRUE;
}

static MMRESULT WINAPI joyGetDevCapsAH(UINT_PTR uJoyID, LPJOYCAPSA pjc, UINT cbjc) {
    return MMSYSERR_NODRIVER;
}

static MMRESULT WINAPI joyGetDevCapsWH(UINT_PTR uJoyID, LPJOYCAPSW pjc, UINT cbjc) {
    return MMSYSERR_NODRIVER;
}

static MMRESULT WINAPI joyGetPosExH(UINT uJoyID, LPJOYINFOEX pji) { return MMSYSERR_NODRIVER; }

static BOOL WINAPI BeepH(DWORD dwFreq, DWORD dwDuration) { return FALSE; }

static HRESULT SHGetSpecialFolderLocationH(HWND hwnd, int csidl, void* ppidl) {
    return static_cast<HRESULT>(1);
}

static int WINAPI WSAStartupH(WORD wVersionRequired, LPWSADATA lpWSAData) {
    // spdlog::warn("Failing WSAStartupH");
    return WSAVERNOTSUPPORTED;
}

static int WINAPI bindH(SOCKET s, const sockaddr* addr, int namelen) { return SOCKET_ERROR; }

void extrahooks::init() {
    HOOK_AUTO("user32.dll", MessageBoxA);
    HOOK_AUTO("user32.dll", MessageBoxW);
    HOOK_AUTO("shell32.dll", DragAcceptFiles);
    HOOK_ONLY("shell32.dll", DragQueryFileA);
    HOOK_ONLY("shell32.dll", DragQueryFileW);
    HOOK_ONLY("shell32.dll", ShellExecuteA);
    HOOK_ONLY("shell32.dll", ShellExecuteW);
    HOOK_ONLY("shell32.dll", ShellExecuteExA);
    HOOK_ONLY("shell32.dll", ShellExecuteExW);
    HOOK_ONLY("shell32.dll", SHGetSpecialFolderLocation);
    HOOK_ONLY("kernel32.dll", EnumSystemLocalesA);
    HOOK_ONLY("kernel32.dll", EnumSystemLocalesW);
    HOOK_ONLY("kernel32.dll", GetUserDefaultLCID);
    HOOK_ONLY("kernel32.dll", GetVersion);
    HOOK_ONLY("kernel32.dll", GetVersionExA);
    HOOK_ONLY("kernel32.dll", GetVersionExW);
    HOOK_ONLY("kernel32.dll", Beep);
    HOOK_ONLY("winmm.dll", joyGetDevCapsA);
    HOOK_ONLY("winmm.dll", joyGetDevCapsW);
    HOOK_ONLY("winmm.dll", joyGetPosEx);
}

void extrahooks::init_net() {
    HOOK_ONLY("ws2_32.dll", WSAStartup);
    HOOK_ONLY("ws2_32.dll", bind);
}
