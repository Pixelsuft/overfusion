#define WIN32_LEAN_AND_MEAN
#include "video.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "files.hpp"
#include "process.hpp"
#include <d3d9.h>
#include <spdlog/spdlog.h>

// Video recording implementation

using std::string;

extern HWND mhwnd;
extern BOOL(WINAPI* BitBltO)(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1,
                             DWORD rop);
extern LRESULT(WINAPI* EditWindowProcO)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
extern BOOL(WINAPI* GetClientRectO)(HWND hWnd, LPRECT lpRect);

namespace video {
enum class CheckResult { Ok, Started, Failed };

// General
static process::Subprocess ffmpeg;
static std::vector<BYTE> data_buffer;
static std::pair<int, int> size;
static int file_index;
static bool recording;
static bool allow_frame;

// Win32 recording
static BITMAPINFO bmi;
static HDC srcdc;
static HDC memdc;
static HBITMAP bmp;
static HGDIOBJ old_bmp;

// D3D9 recording
static LPDIRECT3DSURFACE9 pSysSurface;
} // namespace video

inline bool is_d3d9_cap() {
    auto& cfg = conf::get();
    return cfg.allow_direct_capture && cfg.render_type == conf::RenderType::D3D9;
}

inline bool is_gdi_cap() {
    auto& cfg = conf::get();
    return cfg.allow_direct_capture && cfg.render_type == conf::RenderType::GDI;
}

void video::init() {
    srcdc = nullptr;
    pSysSurface = nullptr;
    allow_frame = false;
    recording = false;
    size.first = size.second = 0;
    file_index = 0;
}

void video::set_allow_frame(bool allow) {
    allow_frame = allow;
    if (allow && memdc && is_gdi_cap()) {
        // Can be useful to see differences
        // PatBlt(memdc, 0, 0, size.first, size.second, BLACKNESS);
    }
}

bool video::is_recording() { return recording; }

void video::start() {
    if (recording)
        return;
    // For sure let's clear them
    srcdc = nullptr;
    pSysSurface = nullptr;
    data_buffer.clear();
    allow_frame = false;
    auto& cfg = conf::get();
    recording = true;
}

void video::stop() {
    if (!recording)
        return;
    auto& cfg = conf::get();
    spdlog::info("Stopping video recording");
    ffmpeg.close();
    recording = false;
    if (is_d3d9_cap()) {
        if (pSysSurface) {
            pSysSurface->Release();
            pSysSurface = nullptr;
        }
        allow_frame = false;
    } else {
        if (!srcdc)
            return;
        SelectObject(memdc, old_bmp);
        DeleteObject(bmp);
        DeleteDC(memdc);
        ReleaseDC(::mhwnd, srcdc);
        srcdc = nullptr;
    }
    data_buffer.clear();
}

static video::CheckResult check_record(std::pair<int, int> new_size) {
    auto& cfg = conf::get();
    auto rec_ret = video::CheckResult::Ok;
    if (video::recording && video::ffmpeg.is_open() && video::size != new_size) {
        spdlog::warn("Window resized, writing to another video file");
        video::size = new_size;
        video::stop();
        video::recording = true;
    }
    if (video::recording && !video::ffmpeg.is_open()) {
        video::size = new_size;
        string cmd = cfg.ffmpeg_cmdline;
        size_t pos;
        while ((pos = cmd.find("$FPS")) != std::string::npos)
            cmd.replace(pos, 4, std::to_string(cfg.fps));
        while ((pos = cmd.find("$SIZE")) != std::string::npos)
            cmd.replace(pos, 5,
                        std::to_string(new_size.first) + "x" + std::to_string(new_size.second));
        while ((pos = cmd.find("$CWD")) != std::string::npos)
            cmd.replace(pos, 4, string(files::get_cwd()));
        while ((pos = cmd.find("$PROJ")) != std::string::npos)
            cmd.replace(pos, 5, cfg.project_name);
        video::file_index++;
        while ((pos = cmd.find("$ID")) != std::string::npos)
            cmd.replace(pos, 3, std::to_string(video::file_index));
        spdlog::info("running: {}", cmd);
        if (!video::ffmpeg.open(cmd)) {
            video::recording = false;
            spdlog::error("Failed to start ffmpeg, stopping recording");
            return video::CheckResult::Failed;
        }
        video::data_buffer.resize(new_size.first * new_size.second * 4);
        rec_ret = video::CheckResult::Started;
    }
    return rec_ret;
}

void* video::get_mem_dc() {
    if (!recording || !is_gdi_cap())
        return nullptr;
    return memdc;
}

static void* last_dev = nullptr; // TODO: remove that
void video::after_draw() {
    auto& cfg = conf::get();
    if (!recording)
        return;
    if (!is_d3d9_cap()) {
        RECT win_rect;
        auto ret = GetClientRectO(::mhwnd, &win_rect);
        ENSURE(ret);
        if (win_rect.right == 0 || win_rect.bottom == 0)
            return;
        switch (check_record({win_rect.right, win_rect.bottom})) {
        case video::CheckResult::Ok:
            break;
        case video::CheckResult::Failed:
            return;
        case video::CheckResult::Started:
            srcdc = GetDC(::mhwnd);
            ENSURE(srcdc != nullptr);
            memdc = CreateCompatibleDC(srcdc);
            ENSURE(memdc != nullptr);
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            bmi.bmiHeader.biClrUsed = 0;
            bmi.bmiHeader.biClrImportant = 0;
            bmi.bmiHeader.biWidth = size.first;
            bmi.bmiHeader.biHeight = -size.second;
            bmp = CreateCompatibleBitmap(srcdc, size.first, size.second);
            ENSURE(bmp != nullptr);
            old_bmp = SelectObject(memdc, bmp);
            if (is_gdi_cap()) {
                ret = PatBlt(memdc, 0, 0, size.first, size.second, BLACKNESS);
                ENSURE(ret);
                // Force window to redraw so we can capture the first frame
                EditWindowProcO(::mhwnd, WM_ERASEBKGND, reinterpret_cast<WPARAM>(srcdc), 0);
            }
            spdlog::info("Window capture started ({}x{})", size.first, size.second);
            break;
        default:
            ASS(false);
        }
        if (!is_gdi_cap()) {
            BOOL success;
            // TODO: configure this in config?
            if (1) {
                success = BitBltO(memdc, 0, 0, size.first, size.second, srcdc, 0, 0,
                                  SRCCOPY | CAPTUREBLT);
            } else {
                success = PrintWindow(::mhwnd, memdc, PW_CLIENTONLY);
            }
            if (!success) {
                spdlog::error("Failed to capture window");
                stop();
                return;
            }
        }
        int bits = GetDIBits(memdc, bmp, 0, size.second, data_buffer.data(), &bmi, DIB_RGB_COLORS);
        ENSURE(bits == size.second);
    }
    if (last_dev && false)
        d3d9_draw(last_dev);
    if (!ffmpeg.write(data_buffer.data(), data_buffer.size())) {
        spdlog::error("Failed to write data to ffmpeg");
        stop();
    }
}

void video::d3d9_draw(void* dev_ptr) {
    auto& cfg = conf::get();
    if (!recording || !is_d3d9_cap() /* || !allow_frame*/)
        return;
    last_dev = dev_ptr;
    allow_frame = false;
    LPDIRECT3DDEVICE9 pDevice = reinterpret_cast<LPDIRECT3DDEVICE9>(dev_ptr);
    LPDIRECT3DSURFACE9 pBackBuffer;
    auto d3d_ret = pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    ENSURE(d3d_ret == D3D_OK);
    D3DSURFACE_DESC desc;
    d3d_ret = pBackBuffer->GetDesc(&desc);
    ENSURE(d3d_ret == D3D_OK);
    switch (check_record({desc.Width, desc.Height})) {
    case video::CheckResult::Ok:
        break;
    case video::CheckResult::Failed:
        pBackBuffer->Release();
        return;
    case video::CheckResult::Started:
        d3d_ret = pDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                       D3DPOOL_SYSTEMMEM, &pSysSurface, nullptr);
        ENSURE(d3d_ret == D3D_OK);
        spdlog::info("Direct3D9 capture started ({}x{})", desc.Width, desc.Height);
        break;
    default:
        ASS(false);
    }
    d3d_ret = pDevice->GetRenderTargetData(pBackBuffer, pSysSurface);
    if (d3d_ret != D3D_OK) {
        spdlog::error("Failed to capture D3D9 frame data");
        pBackBuffer->Release();
        return;
    }
    ASS(data_buffer.size() == desc.Width * desc.Height * 4);
    ASS(data_buffer.size() == size.first * size.second * 4);
    D3DLOCKED_RECT lockedRect;
    d3d_ret = pSysSurface->LockRect(&lockedRect, nullptr, D3DLOCK_READONLY);
    // ENSURE(d3d_ret == D3D_OK);
    if (d3d_ret == D3D_OK) {
        unsigned char* pSrc = reinterpret_cast<unsigned char*>(lockedRect.pBits);
        for (UINT y = 0; y < desc.Height; ++y)
            std::memcpy(&data_buffer[y * desc.Width * 4], pSrc + (y * lockedRect.Pitch),
                        desc.Width * 4);
        pSysSurface->UnlockRect();
    }
    pBackBuffer->Release();
    if (d3d_ret != D3D_OK) {
        spdlog::error("Failed to lock D3D9 surface");
        stop();
    }
}
