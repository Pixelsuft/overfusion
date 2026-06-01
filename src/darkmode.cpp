#define WIN32_LEAN_AND_MEAN
#include "winhooks.hpp"
#include <Windows.h>
#include <vector>
// Should be after Windows.h
#include <CommCtrl.h>
#include <Uxtheme.h>
#include <vsstyle.h>
#pragma comment(lib, "comctl32.lib") // FIXME

// This is win32 dark theme support, including menubar (thanks Microslop!)

#ifndef NTAPI
#define NTAPI __stdcall
#endif

#define LOAD_FUNC_ORD(func_name, func_ord)                                                         \
    do {                                                                                           \
        win_shit.func_name = reinterpret_cast<func_name##_t>(                                      \
            GetProcAddress(uxtheme_handle, MAKEINTRESOURCEA(func_ord)));                           \
        if (win_shit.func_name == nullptr)                                                         \
            return;                                                                                \
    } while (0)

#define WM_UAHDESTROYWINDOW 0x0090
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092
#define WM_UAHINITMENU 0x0093
#define WM_UAHMEASUREMENUITEM 0x0094
#define WM_UAHNCPAINTMENUPOPUP 0x0095

using WIN_NTDLL_NTSTATUS = LONG;

struct WIN_NTDLL_OSVERSIONINFOEXW {
    ULONG dwOSVersionInfoSize;
    ULONG dwMajorVersion;
    ULONG dwMinorVersion;
    ULONG dwBuildNumber;
    ULONG dwPlatformId;
    WCHAR szCSDVersion[128];
    USHORT wServicePackMajor;
    USHORT wServicePackMinor;
    USHORT wSuiteMask;
    UCHAR wProductType;
    UCHAR wReserved;
};

enum WinPreferredAppMode {
    WIN_APPMODE_DEFAULT,
    WIN_APPMODE_ALLOW_DARK,
    WIN_APPMODE_FORCE_DARK,
    WIN_APPMODE_FORCE_LIGHT,
    WIN_APPMODE_MAX
};

enum WINDOWCOMPOSITIONATTRIB {
    WCA_UNDEFINED = 0,
    WCA_NCRENDERING_ENABLED = 1,
    WCA_NCRENDERING_POLICY = 2,
    WCA_TRANSITIONS_FORCEDISABLED = 3,
    WCA_ALLOW_NCPAINT = 4,
    WCA_CAPTION_BUTTON_BOUNDS = 5,
    WCA_NONCLIENT_RTL_LAYOUT = 6,
    WCA_FORCE_ICONIC_REPRESENTATION = 7,
    WCA_EXTENDED_FRAME_BOUNDS = 8,
    WCA_HAS_ICONIC_BITMAP = 9,
    WCA_THEME_ATTRIBUTES = 10,
    WCA_NCRENDERING_EXILED = 11,
    WCA_NCADORNMENTINFO = 12,
    WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
    WCA_VIDEO_OVERLAY_ACTIVE = 14,
    WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
    WCA_DISALLOW_PEEK = 16,
    WCA_CLOAK = 17,
    WCA_CLOAKED = 18,
    WCA_ACCENT_POLICY = 19,
    WCA_FREEZE_REPRESENTATION = 20,
    WCA_EVER_UNCLOAKED = 21,
    WCA_VISUAL_OWNER = 22,
    WCA_HOLOGRAPHIC = 23,
    WCA_EXCLUDED_FROM_DDA = 24,
    WCA_PASSIVEUPDATEMODE = 25,
    WCA_USEDARKMODECOLORS = 26,
    WCA_LAST = 27
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
};

typedef bool(WINAPI* ShouldAppsUseDarkMode_t)(void);
typedef void(WINAPI* AllowDarkModeForWindow_t)(HWND, bool);
typedef void(WINAPI* AllowDarkModeForApp_t)(bool);
typedef void(WINAPI* FlushMenuThemes_t)(void);
typedef void(WINAPI* RefreshImmersiveColorPolicyState_t)(void);
typedef bool(WINAPI* IsDarkModeAllowedForWindow_t)(HWND);
typedef bool(WINAPI* ShouldSystemUseDarkMode_t)(void);
typedef WinPreferredAppMode(WINAPI* SetPreferredAppMode_t)(WinPreferredAppMode);
typedef bool(WINAPI* IsDarkModeAllowedForApp_t)(void);

typedef BOOL(WINAPI* SetWindowCompositionAttribute_t)(HWND, const WINDOWCOMPOSITIONATTRIBDATA*);

typedef WIN_NTDLL_NTSTATUS(NTAPI* RtlGetVersion_t)(WIN_NTDLL_OSVERSIONINFOEXW*);

struct win_shit_type {
    std::vector<HWND> cached_windows;
    ShouldAppsUseDarkMode_t ShouldAppsUseDarkMode;
    AllowDarkModeForWindow_t AllowDarkModeForWindow;
    AllowDarkModeForApp_t AllowDarkModeForApp;
    FlushMenuThemes_t FlushMenuThemes;
    RefreshImmersiveColorPolicyState_t RefreshImmersiveColorPolicyState;
    IsDarkModeAllowedForWindow_t IsDarkModeAllowedForWindow;
    ShouldSystemUseDarkMode_t ShouldSystemUseDarkMode;
    SetPreferredAppMode_t SetPreferredAppMode;
    IsDarkModeAllowedForApp_t IsDarkModeAllowedForApp;
    SetWindowCompositionAttribute_t SetWindowCompositionAttribute;
    HRESULT(WINAPI* CloseThemeData)(HTHEME);
    HRESULT(WINAPI* DrawThemeTextEx)(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, LPRECT,
                                     const DTTOPTS*);
    HTHEME(WINAPI* OpenThemeData)(HWND hwnd, LPCWSTR pszClassList);
    HRESULT(WINAPI* SetWindowTheme)(HWND hwnd, LPCWSTR pszSubAppName, LPCWSTR pszSubIdList);
    RtlGetVersion_t RtlGetVersion;
    HTHEME g_menuTheme;
    HBRUSH g_brBarBackground;
    HBRUSH g_brItemBackground;
    HBRUSH g_brItemBackgroundHot;
    HBRUSH g_brItemBackgroundSelected;
    HBRUSH g_brItemBorder;
    HBRUSH g_hDarkBgBrush;
    DWORD build_num;
    int enabled;
};

union UAHMENUITEMMETRICS {
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizeBar[2];
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizePopup[4];
};

struct UAHMENUPOPUPMETRICS {
    DWORD rgcx[4];
    DWORD fUpdateMaxWidths : 2;
};

struct UAHMENU {
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
};

struct UAHMENUITEM {
    int iPosition;
    UAHMENUITEMMETRICS umim;
    UAHMENUPOPUPMETRICS umpm;
};

struct UAHDRAWMENUITEM {
    DRAWITEMSTRUCT dis;
    UAHMENU um;
    UAHMENUITEM umi;
};

struct UAHMEASUREMENUITEM {
    MEASUREITEMSTRUCT mis;
    UAHMENU um;
    UAHMENUITEM umi;
};

static win_shit_type win_shit;

void winhooks::fix_win32_theme(void* _hwnd) {
    win_shit.cached_windows.push_back(reinterpret_cast<HWND>(_hwnd));
}

void winhooks::init_win32_theme() {
    win_shit.enabled = -1;
    win_shit.g_menuTheme = nullptr;
    win_shit.g_brBarBackground = nullptr;
    win_shit.g_brItemBackground = nullptr;
    win_shit.g_brItemBackgroundHot = nullptr;
    win_shit.g_brItemBackgroundSelected = nullptr;
    win_shit.g_brItemBorder = nullptr;
    win_shit.g_hDarkBgBrush = CreateSolidBrush(RGB(32, 32, 32));
    win_shit.AllowDarkModeForWindow = nullptr;
    win_shit.ShouldAppsUseDarkMode = nullptr;
    win_shit.SetWindowCompositionAttribute = nullptr;
    win_shit.CloseThemeData = nullptr;
    win_shit.DrawThemeTextEx = nullptr;
    win_shit.OpenThemeData = nullptr;
    win_shit.SetWindowTheme = nullptr;
    auto ntdll_handle = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll_handle)
        return;
    win_shit.RtlGetVersion =
        reinterpret_cast<RtlGetVersion_t>(GetProcAddress(ntdll_handle, "RtlGetVersion"));
    if (!win_shit.RtlGetVersion)
        return;
    WIN_NTDLL_OSVERSIONINFOEXW os_ver;
    os_ver.dwOSVersionInfoSize = sizeof(WIN_NTDLL_OSVERSIONINFOEXW);
    win_shit.RtlGetVersion(&os_ver);
    win_shit.build_num = os_ver.dwBuildNumber;
    win_shit.build_num &= ~0xF0000000;
    if (win_shit.build_num < 17763)
        return;
    auto uxtheme_handle = GetModuleHandleW(L"uxtheme.dll");
    if (uxtheme_handle == nullptr)
        return;
    win_shit.CloseThemeData = reinterpret_cast<decltype(win_shit.CloseThemeData)>(
        GetProcAddress(uxtheme_handle, "CloseThemeData"));
    win_shit.DrawThemeTextEx = reinterpret_cast<decltype(win_shit.DrawThemeTextEx)>(
        GetProcAddress(uxtheme_handle, "DrawThemeTextEx"));
    win_shit.OpenThemeData = reinterpret_cast<decltype(win_shit.OpenThemeData)>(
        GetProcAddress(uxtheme_handle, "OpenThemeData"));
    win_shit.SetWindowTheme = reinterpret_cast<decltype(win_shit.SetWindowTheme)>(
        GetProcAddress(uxtheme_handle, "SetWindowTheme"));
    if (win_shit.DrawThemeTextEx == nullptr || win_shit.OpenThemeData == nullptr)
        win_shit.CloseThemeData = nullptr;
    auto user32_handle = GetModuleHandleW(L"user32.dll");
    if (user32_handle == nullptr)
        return;
    win_shit.SetWindowCompositionAttribute = reinterpret_cast<SetWindowCompositionAttribute_t>(
        GetProcAddress(user32_handle, "SetWindowCompositionAttribute"));
    LOAD_FUNC_ORD(ShouldAppsUseDarkMode, 132);
    LOAD_FUNC_ORD(AllowDarkModeForWindow, 133);
    if (win_shit.build_num < 18362) {
        win_shit.SetPreferredAppMode = nullptr;
        LOAD_FUNC_ORD(AllowDarkModeForApp, 135);
    } else {
        win_shit.AllowDarkModeForApp = nullptr;
        LOAD_FUNC_ORD(SetPreferredAppMode, 135);
    }
    LOAD_FUNC_ORD(FlushMenuThemes, 136);
    LOAD_FUNC_ORD(RefreshImmersiveColorPolicyState, 104);
    LOAD_FUNC_ORD(IsDarkModeAllowedForWindow, 137);
    if (win_shit.build_num >= 18290)
        LOAD_FUNC_ORD(ShouldSystemUseDarkMode, 138);
    else
        win_shit.ShouldSystemUseDarkMode = nullptr;
    if (win_shit.build_num >= 18334)
        LOAD_FUNC_ORD(IsDarkModeAllowedForApp, 139);
    else
        win_shit.IsDarkModeAllowedForApp = nullptr;
    if (win_shit.AllowDarkModeForApp != nullptr)
        win_shit.AllowDarkModeForApp(true);
    if (win_shit.SetPreferredAppMode != nullptr)
        win_shit.SetPreferredAppMode(WIN_APPMODE_ALLOW_DARK);
    if (win_shit.RefreshImmersiveColorPolicyState)
        win_shit.RefreshImmersiveColorPolicyState();
}

static LRESULT CALLBACK TrueDarkMessageBoxSubclass(HWND hWnd, UINT uMsg, WPARAM wParam,
                                                   LPARAM lParam, UINT_PTR uIdSubclass,
                                                   DWORD_PTR dwRefData) {
    switch (uMsg) {
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(32, 32, 32));
        return reinterpret_cast<LRESULT>(win_shit.g_hDarkBgBrush);
    }
    case WM_ERASEBKGND: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, win_shit.g_hDarkBgBrush);
        return 1;
    }
    case WM_PAINT: {
        LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
        HDC hdc = GetDC(hWnd);
        if (hdc) {
            RECT rc;
            GetClientRect(hWnd, &rc);
            int trayHeight = 45;
            int borderLineHeight = 1;
            RECT rcTray = {rc.left, rc.bottom - trayHeight, rc.right, rc.bottom};
            RECT rcLine = {rc.left, rc.bottom - trayHeight, rc.right,
                           rc.bottom - trayHeight + borderLineHeight};
            HBRUSH hTrayBrush = CreateSolidBrush(RGB(40, 40, 40));
            HBRUSH hLineBrush = CreateSolidBrush(RGB(70, 70, 70));
            FillRect(hdc, &rcTray, hTrayBrush);
            FillRect(hdc, &rcLine, hLineBrush);
            DeleteObject(hTrayBrush);
            DeleteObject(hLineBrush);
            ReleaseDC(hWnd, hdc);
        }
        return res;
    }
    case WM_DESTROY:
        RemoveWindowSubclass(hWnd, TrueDarkMessageBoxSubclass, uIdSubclass);
        break;
    case WM_INITDIALOG:
    case WM_SHOWWINDOW:
        winhooks::fix_win32_theme_instant(hWnd);
        break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static void fix_win32_theme_real(HWND hwnd) {
    if (!win_shit.AllowDarkModeForWindow)
        return;
    win_shit.AllowDarkModeForWindow(hwnd, true);
    if (win_shit.enabled == -1 && win_shit.ShouldAppsUseDarkMode)
        win_shit.enabled = win_shit.ShouldAppsUseDarkMode() ? 1 : 0;
    bool enable_dark = win_shit.enabled != 0;
    BOOL win_dark = enable_dark ? TRUE : FALSE;
    if (win_shit.build_num < 18362) {
        SetPropW(hwnd, L"UseImmersiveDarkModeColors",
                 reinterpret_cast<HANDLE>(static_cast<INT_PTR>(win_dark)));
    } else if (win_shit.SetWindowCompositionAttribute != nullptr) {
        WINDOWCOMPOSITIONATTRIBDATA data = {WCA_USEDARKMODECOLORS, &win_dark, sizeof(win_dark)};
        win_shit.SetWindowCompositionAttribute(hwnd, &data);
    }
    if (win_shit.SetWindowTheme)
        win_shit.SetWindowTheme(hwnd, enable_dark ? L"DarkMode_Explorer" : nullptr, nullptr);
}

void winhooks::fix_win32_theme_instant(void* _hwnd) {
    fix_win32_theme_real(reinterpret_cast<HWND>(_hwnd));
}

void winhooks::fix_win32_theme_messagebox(void* _hwnd) {
    if (win_shit.enabled == 1)
        SetWindowSubclass(reinterpret_cast<HWND>(_hwnd), TrueDarkMessageBoxSubclass, 0, 0);
}

static void UAHDrawMenuNCBottomLine(HWND hWnd) {
    MENUBARINFO mbi = {sizeof(mbi)};
    if (!GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi)) {
        return;
    }

    RECT rcClient = {0};
    GetClientRect(hWnd, &rcClient);
    MapWindowPoints(hWnd, nullptr, (POINT*)&rcClient, 2);

    RECT rcWindow = {0};
    GetWindowRect(hWnd, &rcWindow);

    OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);

    RECT rcAnnoyingLine = rcClient;
    rcAnnoyingLine.bottom = rcAnnoyingLine.top;
    rcAnnoyingLine.top--;

    HDC hdc = GetWindowDC(hWnd);
    FillRect(hdc, &rcAnnoyingLine, win_shit.g_brBarBackground);
    ReleaseDC(hWnd, hdc);
}

bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr) {
    if (win_shit.CloseThemeData == nullptr)
        return false;
    switch (message) {
    case WM_UAHDRAWMENU: {
        auto pUDM = reinterpret_cast<UAHMENU*>(lParam);
        RECT rc = {0};
        if (1) {
            MENUBARINFO mbi = {sizeof(mbi)};
            GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);
            RECT rcWindow;
            GetWindowRect(hWnd, &rcWindow);
            rc = mbi.rcBar;
            OffsetRect(&rc, -rcWindow.left, -rcWindow.top);
        }
        FillRect(pUDM->hdc, &rc, win_shit.g_brBarBackground);
        return true;
    }
    case WM_UAHDRAWMENUITEM: {
        auto pUDMI = reinterpret_cast<UAHDRAWMENUITEM*>(lParam);
        HBRUSH* pbrBackground = &win_shit.g_brItemBackground;
        HBRUSH* pbrBorder = &win_shit.g_brItemBackground;
        wchar_t menuString[256] = {0};
        MENUITEMINFOW mii = {sizeof(mii), MIIM_STRING};
        {
            mii.dwTypeData = menuString;
            mii.cch = (sizeof(menuString) / 2) - 1;
            GetMenuItemInfoW(pUDMI->um.hmenu, pUDMI->umi.iPosition, TRUE, &mii);
        }
        DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
        int iTextStateID = 0;
        int iBackgroundStateID = 0;
        {
            if ((pUDMI->dis.itemState & ODS_INACTIVE) || (pUDMI->dis.itemState & ODS_DEFAULT)) {
                iTextStateID = MBI_NORMAL;
                iBackgroundStateID = MBI_NORMAL;
            }
            if (pUDMI->dis.itemState & ODS_HOTLIGHT) {
                iTextStateID = MBI_HOT;
                iBackgroundStateID = MBI_HOT;

                pbrBackground = &win_shit.g_brItemBackgroundHot;
                pbrBorder = &win_shit.g_brItemBorder;
            }
            if (pUDMI->dis.itemState & ODS_SELECTED) {
                iTextStateID = MBI_PUSHED;
                iBackgroundStateID = MBI_PUSHED;

                pbrBackground = &win_shit.g_brItemBackgroundSelected;
                pbrBorder = &win_shit.g_brItemBorder;
            }
            if ((pUDMI->dis.itemState & ODS_GRAYED) || (pUDMI->dis.itemState & ODS_DISABLED)) {
                iTextStateID = MBI_DISABLED;
                iBackgroundStateID = MBI_DISABLED;
            }
            if (pUDMI->dis.itemState & ODS_NOACCEL) {
                dwFlags |= DT_HIDEPREFIX;
            }
        }
        if (!win_shit.g_menuTheme)
            win_shit.g_menuTheme = win_shit.OpenThemeData(hWnd, L"Menu");
        DTTOPTS opts = {
            sizeof(opts), DTT_TEXTCOLOR,
            iTextStateID != MBI_DISABLED
                ? (win_shit.enabled == 0 ? RGB(0x00, 0x00, 0x00) : RGB(0xFF, 0xFF, 0xFF))
                : (win_shit.enabled == 0 ? RGB(0x6D, 0x6D, 0x6D) : RGB(0x64, 0x64, 0x64))};

        FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, *pbrBackground);
        FrameRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, *pbrBorder);
        win_shit.DrawThemeTextEx(win_shit.g_menuTheme, pUDMI->um.hdc, MENU_BARITEM, MBI_NORMAL,
                                 menuString, mii.cch, dwFlags, &pUDMI->dis.rcItem, &opts);

        return true;
    }
    case WM_UAHMEASUREMENUITEM: {
        auto pMmi = reinterpret_cast<UAHMEASUREMENUITEM*>(lParam);
        *lr = DefWindowProc(hWnd, message, wParam, lParam);
        pMmi->mis.itemWidth = (pMmi->mis.itemWidth * 4) / 3;
        return true;
    }
    case WM_THEMECHANGED: {
        win_shit.enabled = -1;
        if (win_shit.g_menuTheme) {
            win_shit.CloseThemeData(win_shit.g_menuTheme);
            win_shit.g_menuTheme = nullptr;
        }
        for (auto& win_hwnd : win_shit.cached_windows)
            fix_win32_theme_real(win_hwnd);
        DeleteObject(win_shit.g_brBarBackground);
        DeleteObject(win_shit.g_brItemBackground);
        DeleteObject(win_shit.g_brItemBackgroundHot);
        DeleteObject(win_shit.g_brItemBackgroundSelected);
        DeleteObject(win_shit.g_brItemBorder);
        win_shit.g_brBarBackground = win_shit.enabled == 0 ? CreateSolidBrush(RGB(255, 255, 255))
                                                           : CreateSolidBrush(RGB(25, 25, 25));
        win_shit.g_brItemBackground = win_shit.enabled == 0 ? CreateSolidBrush(RGB(255, 255, 255))
                                                            : CreateSolidBrush(RGB(25, 25, 25));
        win_shit.g_brItemBackgroundHot = win_shit.enabled == 0
                                             ? CreateSolidBrush(RGB(245, 245, 245))
                                             : CreateSolidBrush(RGB(67, 67, 67));
        win_shit.g_brItemBackgroundSelected = win_shit.enabled == 0
                                                  ? CreateSolidBrush(RGB(249, 249, 249))
                                                  : CreateSolidBrush(RGB(33, 33, 33));
        win_shit.g_brItemBorder = win_shit.enabled == 0 ? CreateSolidBrush(RGB(235, 235, 235))
                                                        : CreateSolidBrush(RGB(19, 19, 19));
        return false;
    }
    case WM_NCPAINT:
    case WM_NCACTIVATE:
        *lr = DefWindowProc(hWnd, message, wParam, lParam);
        UAHDrawMenuNCBottomLine(hWnd);
        return true;
    default:
        return false;
    }
}
