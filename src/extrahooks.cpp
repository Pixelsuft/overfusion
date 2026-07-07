#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include "extrahooks.hpp"
#include "config.hpp"
#include "log.hpp"
#include "mem.hpp"
#include "plugbase.hpp"
#include "uconv.hpp"
#include <WinSock2.h>
#include <Windows.h>
#include <shellapi.h>

using ost::string_view;

namespace extrahooks {
static char my_argv_a[MAX_PATH * 2];
static wchar_t my_argv_w[MAX_PATH * 2];
} // namespace extrahooks

static void(WINAPI* DragAcceptFilesO)(HWND hWnd, BOOL fAccept);
static void WINAPI DragAcceptFilesH(HWND hWnd, BOOL fAccept) { DragAcceptFilesO(hWnd, FALSE); }

static UINT WINAPI DragQueryFileWH(HDROP hDrop, UINT iFile, LPWSTR lpszFile, UINT cch) { return 0; }
static UINT WINAPI DragQueryFileAH(HDROP hDrop, UINT iFile, LPSTR lpszFile, UINT cch) { return 0; }

static HINSTANCE WINAPI ShellExecuteAH(HWND hwnd, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir,
                                INT show) {
    of::info("ShellExecuteA: {} {}", uconv::from_ansi(op), uconv::from_ansi(file));
    return reinterpret_cast<HINSTANCE>(SE_ERR_ACCESSDENIED);
}

static HINSTANCE WINAPI ShellExecuteWH(HWND hwnd, LPCWSTR op, LPCWSTR file, LPCWSTR params, LPCWSTR dir,
                                INT show) {
    of::info("ShellExecuteW: {} {}", uconv::from_utf16(op), uconv::from_utf16(file));
    return reinterpret_cast<HINSTANCE>(SE_ERR_ACCESSDENIED);
}

static BOOL WINAPI ShellExecuteExAH(SHELLEXECUTEINFOA* pExecInfo) {
    of::info("ShellExecuteExA: {} {}", uconv::from_ansi(pExecInfo->lpVerb),
             uconv::from_ansi(pExecInfo->lpFile));
    return FALSE;
}

static BOOL WINAPI ShellExecuteExWH(SHELLEXECUTEINFOW* pExecInfo) {
    of::info("ShellExecuteExW: {} {}", uconv::from_utf16(pExecInfo->lpVerb),
             uconv::from_utf16(pExecInfo->lpFile));
    return FALSE;
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
    if (lpVersionInformation == nullptr)
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
    if (lpVersionInformation == nullptr)
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

static UINT WINAPI joyGetDevCapsAH(UINT_PTR uJoyID, const void* pjc, UINT cbjc) {
    of::warn("Failing joyGetDevCapsA");
    return 6;
}

static UINT WINAPI joyGetDevCapsWH(UINT_PTR uJoyID, const void* pjc, UINT cbjc) {
    of::warn("Failing joyGetDevCapsW");
    return 6;
}

static UINT WINAPI joyGetPosExH(UINT uJoyID, const void* pji) {
    of::warn("Failing joyGetPosEx");
    return 6;
}

static HRESULT WINAPI SHGetSpecialFolderLocationH(HWND hwnd, int csidl, void* ppidl) {
    return static_cast<HRESULT>(1);
}

static int WINAPI WSAStartupH(WORD wVersionRequired, LPWSADATA lpWSAData) {
    of::warn("Networking is not supported - Failing WSAStartup");
    return WSAVERNOTSUPPORTED;
}

static int WINAPI bindH(SOCKET s, const sockaddr* addr, int namelen) { return SOCKET_ERROR; }

static BOOL WINAPI GetUserNameAH(LPSTR lpBuffer, LPDWORD pcbBuffer) {
    if (!pcbBuffer)
        return FALSE;
    *pcbBuffer = 11;
    if (!lpBuffer) {
        *pcbBuffer = 11;
        return FALSE;
    }
    of::info("Faking user name: OverFusion");
    strcpy(lpBuffer, "OverFusion");
    return TRUE;
}

static BOOL WINAPI GetUserNameWH(LPWSTR lpBuffer, LPDWORD pcbBuffer) {
    if (!pcbBuffer)
        return FALSE;
    *pcbBuffer = 11;
    if (!lpBuffer) {
        *pcbBuffer = 11;
        return FALSE;
    }
    of::info("Faking user name: OverFusion");
    wcscpy(lpBuffer, L"OverFusion");
    return TRUE;
}

static LSTATUS WINAPI RegOpenKeyExAH(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired,
                                     PHKEY phkResult) {
    of::warn("RegOpenKeyExA: Blocked access to {}",
             lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegOpenKeyExWH(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions,
                                     REGSAM samDesired, PHKEY phkResult) {
    of::warn("RegOpenKeyExW: Blocked access to {}",
             lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegCreateKeyExAH(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass,
                                       DWORD dwOptions, REGSAM samDesired,
                                       LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult,
                                       LPDWORD lpdwDisposition) {
    of::warn("RegCreateKeyExA: Blocked access to {}",
             lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegCreateKeyExWH(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass,
                                       DWORD dwOptions, REGSAM samDesired,
                                       LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult,
                                       LPDWORD lpdwDisposition) {
    of::warn("RegCreateKeyExW: Blocked access to {}",
             lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegQueryValueExAH(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                                        LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    of::warn("RegQueryValueExA: Blocked access to {}",
             lpValueName ? uconv::from_ansi(lpValueName) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegQueryValueExWH(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved,
                                        LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    of::warn("RegQueryValueExW: Blocked access to {}",
             lpValueName ? uconv::from_utf16(lpValueName) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegOpenKeyAH(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) {
    of::warn("RegOpenKeyA: Blocked access to {}",
             lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegOpenKeyWH(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult) {
    of::warn("RegOpenKeyW: Blocked access to {}",
             lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegQueryValueAH(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData) {
    of::warn("RegQueryValueA: Blocked access to {}",
             lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegQueryValueWH(HKEY hKey, LPCWSTR lpSubKey, LPWSTR lpData, PLONG lpcbData) {
    of::warn("RegQueryValueW: Blocked access to {}",
             lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegCreateKeyAH(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult) {
    of::warn("RegCreateKeyA: Blocked access to {}",
             lpSubKey ? uconv::from_ansi(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static LSTATUS WINAPI RegCreateKeyWH(HKEY hKey, LPCWSTR lpSubKey, PHKEY phkResult) {
    of::warn("RegCreateKeyW: Blocked access to {}",
             lpSubKey ? uconv::from_utf16(lpSubKey) : "nullptr");
    return ERROR_ACCESS_DENIED;
}

static BOOL WINAPI InternetGetConnectedStateH(LPDWORD lpdwFlags, DWORD dwReserved) {
    of::debug("Faking InternetGetConnectedState returning an error");
    *lpdwFlags = 0x20;
    return FALSE;
}

static LPSTR WINAPI GetCommandLineAH() {
    // of::debug("GetCommandLineAH {}", my_argv_a);
    return extrahooks::my_argv_a;
}

static LPWSTR WINAPI GetCommandLineWH() {
    // of::debug("GetCommandLineWH {}", uconv::from_utf16(my_argv_w));
    return extrahooks::my_argv_w;
}

void extrahooks::init() {
    [] {
        auto& cfg = conf::get();
        strcpy(my_argv_a, GetCommandLineA());
        wcscpy(my_argv_w, GetCommandLineW());
        auto str_cmd = plug::get().cmdline_append + cfg.cmdline_append;
        if (cfg.disable_fullscreen)
            str_cmd += " /NOF";
        auto temp1 = uconv::to_ansi(str_cmd);
        strcat(my_argv_a, temp1);
        std::free(temp1);
        auto temp2 = uconv::to_utf16(str_cmd);
        wcscat(my_argv_w, temp2);
        std::free(temp2);
        auto addr = mem::get_addr("msvcrt.dll", "_acmdln");
        if (addr)
            *reinterpret_cast<char**>(addr) = my_argv_a;
        addr = mem::get_addr("msvcrt.dll", "_wcmdln");
        if (addr)
            *reinterpret_cast<wchar_t**>(addr) = my_argv_w;
        of::info("Command line: {}", uconv::from_utf16(my_argv_w));
    }();
    // TODO: GetDateFormatEx, GetLocaleInfoEx, GetTimeFormatEx, GetUserDefaultLocaleName
    IAT_AUTO("shell32.dll", DragAcceptFiles);
    IAT_STR_ONLY("shell32.dll", DragQueryFile);
    IAT_STR_ONLY("shell32.dll", ShellExecute);
    IAT_STR_ONLY("shell32.dll", ShellExecuteEx);
    IAT_STR_ONLY("kernel32.dll", GetCommandLine);
    IAT_STR_ONLY("kernel32.dll", GetVersionEx);
    IAT_STR_ONLY("kernel32.dll", EnumSystemLocales);
    IAT_ONLY("shell32.dll", SHGetSpecialFolderLocation);
    IAT_ONLY("kernel32.dll", GetUserDefaultLCID);
    IAT_ONLY("kernel32.dll", GetVersion);
    IAT_STR_ONLY("winmm.dll", joyGetDevCaps);
    IAT_ONLY("winmm.dll", joyGetPosEx);
}

void extrahooks::init_ws32() {
    IAT_ONLY("ws2_32.dll", WSAStartup);
    IAT_ONLY("ws2_32.dll", bind);
}

void extrahooks::init_inet() { IAT_ONLY("wininet.dll", InternetGetConnectedState); }

void extrahooks::init_adv() {
    IAT_STR_ONLY("advapi32.dll", GetUserName);
    IAT_STR_ONLY("advapi32.dll", RegOpenKeyEx);
    IAT_STR_ONLY("advapi32.dll", RegCreateKeyEx);
    IAT_STR_ONLY("advapi32.dll", RegQueryValueEx);
    IAT_STR_ONLY("advapi32.dll", RegOpenKey);
    IAT_STR_ONLY("advapi32.dll", RegCreateKey);
    IAT_STR_ONLY("advapi32.dll", RegQueryValue);
}
