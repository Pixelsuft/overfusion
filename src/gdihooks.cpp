#define WIN32_LEAN_AND_MEAN
#include "gdihooks.hpp"
#include "mem.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

extern HWND hwnd;
extern HWND mhwnd;

static BOOL(WINAPI* BitBltO)(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1,
                             DWORD rop);
static BOOL WINAPI BitBltH(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1,
                           DWORD rop) {
    auto ret = BitBltO(hdc, x, y, cx, cy, hdcSrc, x1, y1, rop);
    if (rop == SRCCOPY) {
        // spdlog::debug("BitBlt {} {} {} {} {} {}", x, y, cx, cy, x1, y1);
    }
    return ret;
}

void gdihooks::init() { IAT_AUTO("gdi32.dll", BitBlt); }
