#define WIN32_LEAN_AND_MEAN
#include "gdihooks.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "mem.hpp"
#include "video.hpp"
#include <Windows.h>
#include <spdlog/spdlog.h>

extern HWND mhwnd;

BOOL(WINAPI* BitBltO)(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1,
                      DWORD rop) = BitBlt;
static BOOL WINAPI BitBltH(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1,
                           DWORD rop) {
    auto ret = BitBltO(hdc, x, y, cx, cy, hdcSrc, x1, y1, rop);
    if (rop == SRCCOPY && ::mhwnd && WindowFromDC(hdc) == ::mhwnd) {
        // spdlog::debug("BitBlt {} {} {} {} {} {}", x, y, cx, cy, x1, y1);
        // Late setting but should be OK
        auto& cfg = conf::get();
        ASS(cfg.render_type == conf::RenderType::None || cfg.render_type == conf::RenderType::GDI);
        cfg.render_type = conf::RenderType::GDI;
        auto mem_dc = reinterpret_cast<HDC>(video::get_mem_dc());
        if (mem_dc) {
            auto ret2 = BitBltO(mem_dc, x, y, cx, cy, hdcSrc, x1, y1, rop);
            ENSURE(ret2);
        }
    }
    return ret;
}

static BOOL(WINAPI* StretchBltO)(HDC hdcDest, int xDest, int yDest, int wDest, int hDest,
                                 HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop);
static BOOL WINAPI StretchBltH(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc,
                               int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop) {
    auto ret =
        StretchBltO(hdcDest, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc, hSrc, rop);
    if (rop == SRCCOPY && ::mhwnd && WindowFromDC(hdcDest) == ::mhwnd) {
        // spdlog::debug("StretchBlt {},{},{},{} <- {},{},{},{}", xDest, yDest, wDest, hDest, xSrc,
        // ySrc, wSrc, hSrc);
        auto& cfg = conf::get();
        ASS(cfg.render_type == conf::RenderType::None || cfg.render_type == conf::RenderType::GDI);
        cfg.render_type = conf::RenderType::GDI;
        auto mem_dc = reinterpret_cast<HDC>(video::get_mem_dc());
        if (mem_dc) {
            auto ret2 = StretchBltO(mem_dc, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc,
                                    hSrc, rop);
            ENSURE(ret2);
        }
    }
    return ret;
}

void gdihooks::init() {
    IAT_AUTO("gdi32.dll", BitBlt);
    IAT_AUTO("gdi32.dll", StretchBlt);
}
