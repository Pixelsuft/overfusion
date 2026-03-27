#define WIN32_LEAN_AND_MEAN
#include "winhooks.hpp"
#include <Windows.h>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <vector>
// Should be after Windows.h
#include <Uxtheme.h>
#include <vsstyle.h>
#pragma comment(lib, "uxtheme.lib")
// TODO: support < that Windows Vista

#ifndef NTAPI
#define NTAPI __stdcall
#endif

#define LOAD_FUNC_ORD(func_name, func_ord)                                                         \
    do {                                                                                           \
        win_shit.func_name = reinterpret_cast<func_name##_t>(                                      \
            GetProcAddress(win_shit.uxtheme_handle, MAKEINTRESOURCEA(func_ord)));                  \
        if (win_shit.func_name == nullptr) {                                                       \
            FreeLibrary(win_shit.uxtheme_handle);                                                  \
            return true;                                                                           \
        }                                                                                          \
    } while (0)

#define WM_UAHDESTROYWINDOW 0x0090
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092
#define WM_UAHINITMENU 0x0093
#define WM_UAHMEASUREMENUITEM 0x0094
#define WM_UAHNCPAINTMENUPOPUP 0x0095

typedef LONG WIN_NTDLL_NTSTATUS;

typedef struct {
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
} WIN_NTDLL_OSVERSIONINFOEXW;

typedef enum {
    WIN_APPMODE_DEFAULT,
    WIN_APPMODE_ALLOW_DARK,
    WIN_APPMODE_FORCE_DARK,
    WIN_APPMODE_FORCE_LIGHT,
    WIN_APPMODE_MAX
} WinPreferredAppMode;

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

typedef struct {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

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

typedef struct {
    HMODULE uxtheme_handle;
    HMODULE user32_handle;
    HMODULE ntdll_handle;
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
    RtlGetVersion_t RtlGetVersion;
    DWORD build_num;
} win_shit_type;

typedef union tagUAHMENUITEMMETRICS {
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizeBar[2];
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizePopup[4];
} UAHMENUITEMMETRICS;

typedef struct tagUAHMENUPOPUPMETRICS {
    DWORD rgcx[4];
    DWORD fUpdateMaxWidths : 2;
} UAHMENUPOPUPMETRICS;

typedef struct tagUAHMENU {
    HMENU hmenu;
    HDC hdc;
    DWORD dwFlags;
} UAHMENU;

typedef struct tagUAHMENUITEM {
    int iPosition;
    UAHMENUITEMMETRICS umim;
    UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM;

typedef struct UAHDRAWMENUITEM {
    DRAWITEMSTRUCT dis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHDRAWMENUITEM;

typedef struct tagUAHMEASUREMENUITEM {
    MEASUREITEMSTRUCT mis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHMEASUREMENUITEM;

static int dark_mode_enabled = -1;
static HTHEME g_menuTheme = nullptr;
static HBRUSH g_brBarBackground = nullptr;
static HBRUSH g_brItemBackground = nullptr;
static HBRUSH g_brItemBackgroundHot = nullptr;
static HBRUSH g_brItemBackgroundSelected = nullptr;
static HBRUSH g_brItemBorder = nullptr;
static std::vector<HWND> cached_windows;

void winhooks::fix_win32_theme(void* _hwnd) {
    cached_windows.push_back(reinterpret_cast<HWND>(_hwnd));
}

static bool fix_win32_theme_real(HWND hwnd) {
    // Just copy-pasted code from my game
    win_shit_type win_shit;
    win_shit.uxtheme_handle = nullptr;
    win_shit.user32_handle = nullptr;
    win_shit.ntdll_handle = GetModuleHandleW(L"ntdll.dll");
    if (!win_shit.ntdll_handle)
        return false;
    win_shit.RtlGetVersion =
        (RtlGetVersion_t)GetProcAddress(win_shit.ntdll_handle, "RtlGetVersion");
    if (!win_shit.RtlGetVersion)
        return false;
    WIN_NTDLL_OSVERSIONINFOEXW os_ver;
    os_ver.dwOSVersionInfoSize = sizeof(WIN_NTDLL_OSVERSIONINFOEXW);
    win_shit.RtlGetVersion(&os_ver);
    win_shit.build_num = os_ver.dwBuildNumber;
    win_shit.build_num &= ~0xF0000000;
    if (win_shit.build_num < 17763)
        return true; // Lol ur windoes is too old;
    win_shit.uxtheme_handle = GetModuleHandleW(L"uxtheme.dll");
    if (win_shit.uxtheme_handle == nullptr)
        return false;
    win_shit.user32_handle = GetModuleHandleW(L"user32.dll");
    if (win_shit.user32_handle == nullptr)
        return false;
    win_shit.SetWindowCompositionAttribute = (SetWindowCompositionAttribute_t)GetProcAddress(
        win_shit.user32_handle, "SetWindowCompositionAttribute");
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
    // Let's begin our magic
    if (win_shit.AllowDarkModeForApp != nullptr)
        win_shit.AllowDarkModeForApp(true);
    if (win_shit.SetPreferredAppMode != nullptr)
        win_shit.SetPreferredAppMode(WIN_APPMODE_ALLOW_DARK);
    win_shit.RefreshImmersiveColorPolicyState();
    win_shit.AllowDarkModeForWindow(hwnd, true);
    if (dark_mode_enabled == -1)
        dark_mode_enabled = win_shit.ShouldAppsUseDarkMode() ? 1 : 0;
    bool enable_dark = dark_mode_enabled != 0;
    BOOL win_dark = enable_dark ? TRUE : FALSE;
    if (win_shit.build_num < 18362) {
        SetPropW(hwnd, L"UseImmersiveDarkModeColors",
                 reinterpret_cast<HANDLE>(static_cast<INT_PTR>(win_dark)));
    } else if (win_shit.SetWindowCompositionAttribute != nullptr) {
        WINDOWCOMPOSITIONATTRIBDATA data = {WCA_USEDARKMODECOLORS, &win_dark, sizeof(win_dark)};
        win_shit.SetWindowCompositionAttribute(hwnd, &data);
    }
    return true;
}

void UAHDrawMenuNCBottomLine(HWND hWnd) {
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
    FillRect(hdc, &rcAnnoyingLine, g_brBarBackground);
    ReleaseDC(hWnd, hdc);
}

bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr) {
    switch (message) {
    case WM_UAHDRAWMENU: {
        UAHMENU* pUDM = (UAHMENU*)lParam;
        RECT rc = {0};
        if (1) {
            MENUBARINFO mbi = {sizeof(mbi)};
            GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);
            RECT rcWindow;
            GetWindowRect(hWnd, &rcWindow);
            rc = mbi.rcBar;
            OffsetRect(&rc, -rcWindow.left, -rcWindow.top);
        }
        FillRect(pUDM->hdc, &rc, g_brBarBackground);
        return true;
    }
    case WM_UAHDRAWMENUITEM: {
        UAHDRAWMENUITEM* pUDMI = (UAHDRAWMENUITEM*)lParam;
        HBRUSH* pbrBackground = &g_brItemBackground;
        HBRUSH* pbrBorder = &g_brItemBackground;
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

                pbrBackground = &g_brItemBackgroundHot;
                pbrBorder = &g_brItemBorder;
            }
            if (pUDMI->dis.itemState & ODS_SELECTED) {
                iTextStateID = MBI_PUSHED;
                iBackgroundStateID = MBI_PUSHED;

                pbrBackground = &g_brItemBackgroundSelected;
                pbrBorder = &g_brItemBorder;
            }
            if ((pUDMI->dis.itemState & ODS_GRAYED) || (pUDMI->dis.itemState & ODS_DISABLED)) {
                iTextStateID = MBI_DISABLED;
                iBackgroundStateID = MBI_DISABLED;
            }
            if (pUDMI->dis.itemState & ODS_NOACCEL) {
                dwFlags |= DT_HIDEPREFIX;
            }
        }

        if (!g_menuTheme) {
            g_menuTheme = OpenThemeData(hWnd, L"Menu");
        }

        DTTOPTS opts = {sizeof(opts), DTT_TEXTCOLOR,
                        iTextStateID != MBI_DISABLED ? (dark_mode_enabled == 0 ? RGB(0x00, 0x00, 0x00) : RGB(0xFF, 0xFF, 0xFF))
                                                     : (dark_mode_enabled == 0 ? RGB(0x6D, 0x6D, 0x6D) : RGB(0x64, 0x64, 0x64))};

        FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, *pbrBackground);
        FrameRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, *pbrBorder);
        DrawThemeTextEx(g_menuTheme, pUDMI->um.hdc, MENU_BARITEM, MBI_NORMAL, menuString, mii.cch,
                        dwFlags, &pUDMI->dis.rcItem, &opts);

        return true;
    }
    case WM_UAHMEASUREMENUITEM: {
        UAHMEASUREMENUITEM* pMmi = (UAHMEASUREMENUITEM*)lParam;
        *lr = DefWindowProc(hWnd, message, wParam, lParam);
        pMmi->mis.itemWidth = (pMmi->mis.itemWidth * 4) / 3;
        return true;
    }
    case WM_THEMECHANGED: {
        dark_mode_enabled = -1;
        if (g_menuTheme) {
            CloseThemeData(g_menuTheme);
            g_menuTheme = nullptr;
        }
        for (auto& win_hwnd : cached_windows)
            fix_win32_theme_real(win_hwnd);
        DeleteObject(g_brBarBackground);
        DeleteObject(g_brItemBackground);
        DeleteObject(g_brItemBackgroundHot);
        DeleteObject(g_brItemBackgroundSelected);
        DeleteObject(g_brItemBorder);
        g_brBarBackground = dark_mode_enabled == 0 ? CreateSolidBrush(RGB(255, 255, 255)) : CreateSolidBrush(RGB(25, 25, 25));
        g_brItemBackground = dark_mode_enabled == 0 ? CreateSolidBrush(RGB(255, 255, 255)) : CreateSolidBrush(RGB(25, 25, 25));
        g_brItemBackgroundHot = dark_mode_enabled == 0 ? CreateSolidBrush(RGB(245, 245, 245)) : CreateSolidBrush(RGB(67, 67, 67));
        g_brItemBackgroundSelected = dark_mode_enabled == 0 ? CreateSolidBrush(RGB(249, 249, 249)) : CreateSolidBrush(RGB(33, 33, 33));
        g_brItemBorder = dark_mode_enabled == 0 ? CreateSolidBrush(RGB(235, 235, 235)) : CreateSolidBrush(RGB(19, 19, 19));
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
