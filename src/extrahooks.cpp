#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include "extrahooks.hpp"
#include "config.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "uconv.hpp"
#include "ui.hpp"
#include <WinSock2.h>
#include <Windows.h>
#include <joystickapi.h>
#include <shellapi.h>
#include <spdlog/spdlog.h>

using ost::string_view;

static char my_argv_a[MAX_PATH * 2];
static wchar_t my_argv_w[MAX_PATH * 2];

static void(WINAPI* DragAcceptFilesO)(HWND hWnd, BOOL fAccept);
static void WINAPI DragAcceptFilesH(HWND hWnd, BOOL fAccept) { DragAcceptFilesO(hWnd, FALSE); }

static UINT DragQueryFileWH(HDROP hDrop, UINT iFile, LPWSTR lpszFile, UINT cch) { return 0; }
static UINT DragQueryFileAH(HDROP hDrop, UINT iFile, LPSTR lpszFile, UINT cch) { return 0; }

static HINSTANCE ShellExecuteAH(HWND hwnd, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir,
                                INT show) {
    spdlog::info("ShellExecuteA: {} {}", uconv::from_ansi(op), uconv::from_ansi(file));
    return reinterpret_cast<HINSTANCE>(SE_ERR_ACCESSDENIED);
}

static HINSTANCE ShellExecuteWH(HWND hwnd, LPCWSTR op, LPCWSTR file, LPCWSTR params, LPCWSTR dir,
                                INT show) {
    spdlog::info("ShellExecuteW: {} {}", uconv::from_utf16(op), uconv::from_utf16(file));
    return reinterpret_cast<HINSTANCE>(SE_ERR_ACCESSDENIED);
}

static BOOL ShellExecuteExAH(SHELLEXECUTEINFOA* pExecInfo) {
    spdlog::info("ShellExecuteExA: {} {}", uconv::from_ansi(pExecInfo->lpVerb),
                 uconv::from_ansi(pExecInfo->lpFile));
    return FALSE;
}

static BOOL ShellExecuteExWH(SHELLEXECUTEINFOW* pExecInfo) {
    spdlog::info("ShellExecuteExW: {} {}", uconv::from_utf16(pExecInfo->lpVerb),
                 uconv::from_utf16(pExecInfo->lpFile));
    return FALSE;
}

static int process_message_box(string_view text, string_view caption, UINT uType) {
    if (ui::processing)
        return 0;
    if (uType == 0x30 && state::invalidate_process())
        return IDOK;
    if (caption == "Microsoft Visual C++ Runtime Library")
        return 0;
    spdlog::info("MessageBox: {} - {}", caption, text);
    return 0;
}

static int(WINAPI* MessageBoxAO)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
static int WINAPI MessageBoxAH(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
    auto ret = process_message_box(uconv::from_ansi(lpText), uconv::from_ansi(lpCaption), uType);
    if (ret == 0)
        ret = MessageBoxAO(hWnd, lpText, lpCaption, uType);
    return ret;
}

static int(WINAPI* MessageBoxWO)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
static int WINAPI MessageBoxWH(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    auto ret = process_message_box(uconv::from_utf16(lpText), uconv::from_utf16(lpCaption), uType);
    if (ret == 0)
        ret = MessageBoxWO(hWnd, lpText, lpCaption, uType);
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
    spdlog::warn("Failing joyGetDevCapsA");
    return MMSYSERR_NODRIVER;
}

static MMRESULT WINAPI joyGetDevCapsWH(UINT_PTR uJoyID, LPJOYCAPSW pjc, UINT cbjc) {
    spdlog::warn("Failing joyGetDevCapsW");
    return MMSYSERR_NODRIVER;
}

static MMRESULT WINAPI joyGetPosExH(UINT uJoyID, LPJOYINFOEX pji) {
    spdlog::warn("Failing joyGetPosEx");
    return MMSYSERR_NODRIVER;
}

static BOOL WINAPI BeepH(DWORD dwFreq, DWORD dwDuration) {
    spdlog::info("Beep (freq={}, duration={})", dwFreq, dwDuration);
    return FALSE;
}

static HRESULT SHGetSpecialFolderLocationH(HWND hwnd, int csidl, void* ppidl) {
    return static_cast<HRESULT>(1);
}

static int WINAPI WSAStartupH(WORD wVersionRequired, LPWSADATA lpWSAData) {
    spdlog::warn("Networking is not supported - Failing WSAStartup");
    return WSAVERNOTSUPPORTED;
}

static int WINAPI bindH(SOCKET s, const sockaddr* addr, int namelen) { return SOCKET_ERROR; }

static BOOL __stdcall GetUserNameAH(LPSTR lpBuffer, LPDWORD pcbBuffer) {
    spdlog::info("Faking user name: OverFusion");
    strcpy(lpBuffer, "OverFusion");
    return TRUE;
}

static BOOL __stdcall GetUserNameWH(LPWSTR lpBuffer, LPDWORD pcbBuffer) {
    spdlog::info("Faking user name: OverFusion");
    wcscpy(lpBuffer, L"OverFusion");
    return TRUE;
}

static LSTATUS WINAPI RegOpenKeyExAH(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired,
                                     PHKEY phkResult) {
    spdlog::warn("RegOpenKeyExA: Blocked access to {}",
                 lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegOpenKeyExWH(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions,
                                     REGSAM samDesired, PHKEY phkResult) {
    spdlog::warn("RegOpenKeyExW: Blocked access to {}",
                 lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegCreateKeyExAH(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass,
                                       DWORD dwOptions, REGSAM samDesired,
                                       LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult,
                                       LPDWORD lpdwDisposition) {
    spdlog::warn("RegCreateKeyExA: Blocked access to {}",
                 lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegCreateKeyExWH(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass,
                                       DWORD dwOptions, REGSAM samDesired,
                                       LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult,
                                       LPDWORD lpdwDisposition) {
    spdlog::warn("RegCreateKeyExW: Blocked access to {}",
                 lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegQueryValueExAH(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                                        LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    spdlog::warn("RegQueryValueExA: Blocked access to {}",
                 lpValueName ? uconv::from_ansi(lpValueName) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegQueryValueExWH(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved,
                                        LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    spdlog::warn("RegQueryValueExW: Blocked access to {}",
                 lpValueName ? uconv::from_utf16(lpValueName) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegOpenKeyAH(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) {
    spdlog::warn("RegOpenKeyA: Blocked access to {}",
                 lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegOpenKeyWH(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult) {
    spdlog::warn("RegOpenKeyW: Blocked access to {}",
                 lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegQueryValueAH(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData) {
    spdlog::warn("RegQueryValueA: Blocked access to {}",
                 lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegQueryValueWH(HKEY hKey, LPCWSTR lpSubKey, LPWSTR lpData, PLONG lpcbData) {
    spdlog::warn("RegQueryValueW: Blocked access to {}",
                 lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegCreateKeyAH(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) {
    spdlog::warn("RegCreateKeyA: Blocked access to {}",
                 lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegCreateKeyWH(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult) {
    spdlog::warn("RegCreateKeyW: Blocked access to {}",
                 lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static BOOL __stdcall InternetGetConnectedStateH(LPDWORD lpdwFlags, DWORD dwReserved) {
    spdlog::debug("Faking InternetGetConnectedState returning an error");
    *lpdwFlags = 0x20;
    return FALSE;
}

static LPSTR GetCommandLineAH() {
    // spdlog::debug("GetCommandLineAH {}", my_argv_a);
    return my_argv_a;
}

static LPWSTR GetCommandLineWH() {
    // spdlog::debug("GetCommandLineWH {}", uconv::from_utf16(my_argv_w));
    return my_argv_w;
}

void extrahooks::init() {
    [] {
        strcpy(my_argv_a, GetCommandLineA());
        wcscpy(my_argv_w, GetCommandLineW());
        auto temp1 = uconv::to_ansi(plug::get().cmdline_append + conf::get().cmdline_append);
        strcat(my_argv_a, temp1);
        std::free(temp1);
        auto temp2 = uconv::to_utf16(plug::get().cmdline_append + conf::get().cmdline_append);
        wcscat(my_argv_w, temp2);
        std::free(temp2);
        // TODO: better way than GetModuleHandleA
        *reinterpret_cast<char**>(GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_acmdln")) =
            my_argv_a;
        *reinterpret_cast<wchar_t**>(GetProcAddress(GetModuleHandleA("msvcrt.dll"), "_wcmdln")) =
            my_argv_w;
    }();
    // TODO: GetDateFormatEx, GetLocaleInfoEx, GetTimeFormatEx, GetUserDefaultLocaleName
    HOOK_STR_AUTO("user32.dll", MessageBox);
    HOOK_AUTO("shell32.dll", DragAcceptFiles);
    HOOK_STR_ONLY("shell32.dll", DragQueryFile);
    HOOK_STR_ONLY("shell32.dll", ShellExecute);
    HOOK_STR_ONLY("shell32.dll", ShellExecuteEx);
    HOOK_STR_ONLY("kernel32.dll", GetCommandLine);
    HOOK_STR_ONLY("kernel32.dll", GetVersionEx);
    HOOK_STR_ONLY("kernel32.dll", EnumSystemLocales);
    // FIXME: crashes FNAF (kcini.mfx)
    // HOOK_ONLY("shell32.dll", SHGetSpecialFolderLocation);
    HOOK_ONLY("kernel32.dll", GetUserDefaultLCID);
    HOOK_ONLY("kernel32.dll", GetVersion);
    HOOK_ONLY("kernel32.dll", Beep);
    HOOK_STR_ONLY("winmm.dll", joyGetDevCaps);
    HOOK_ONLY("winmm.dll", joyGetPosEx);
}

void extrahooks::init_ws32() {
    HOOK_ONLY("ws2_32.dll", WSAStartup);
    HOOK_ONLY("ws2_32.dll", bind);
}

void extrahooks::init_inet() { HOOK_ONLY("wininet.dll", InternetGetConnectedState); }

void extrahooks::init_adv() {
    HOOK_STR_ONLY("advapi32.dll", GetUserName);
    HOOK_STR_ONLY("advapi32.dll", RegOpenKeyEx);
    HOOK_STR_ONLY("advapi32.dll", RegCreateKeyEx);
    HOOK_STR_ONLY("advapi32.dll", RegQueryValueEx);
    HOOK_STR_ONLY("advapi32.dll", RegOpenKey);
    HOOK_STR_ONLY("advapi32.dll", RegCreateKey);
    HOOK_STR_ONLY("advapi32.dll", RegQueryValue);
}
