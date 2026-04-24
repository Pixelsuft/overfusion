#define WIN32_LEAN_AND_MEAN
#include "video.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "subprocess.hpp"
#include <d3d9.h>
#include <spdlog/spdlog.h>

// TODO: replace many ENSUREs with normal checks

using std::string;

extern HWND hwnd;

namespace video {
enum class CheckResult { Ok, Started, Failed };

// General
static subprocess::Process ffmpeg;
static std::vector<BYTE> data_buffer;
static std::pair<int, int> size;
static bool use_d3d9;
static bool recording;

// Win32 recording
static BITMAPINFO bmi;
static HDC srcdc;
static HDC memdc;
static HBITMAP bmp;
static HGDIOBJ old_bmp;

// D3D9 recording
static LPDIRECT3DSURFACE9 pSysSurface;
static bool allow_d3d9_frame;
} // namespace video

void video::init() {
    srcdc = nullptr;
    pSysSurface = nullptr;
    use_d3d9 = false;
    allow_d3d9_frame = false;
    recording = false;
    size.first = size.second = 0;
}

void video::set_allow_d3d9_frame(bool allow) { allow_d3d9_frame = allow; }

void video::start() {
    if (recording)
        return;
    // For sure let's clear them
    srcdc = nullptr;
    pSysSurface = nullptr;
    data_buffer.clear();
    allow_d3d9_frame = false;
    auto& cfg = conf::get();
    recording = true;
    use_d3d9 = cfg.alllow_d3d9_recording && !cfg.custom_window;
}

void video::stop() {
    if (!recording)
        return;
    spdlog::info("Stopping video recording");
    ffmpeg.close();
    recording = false;
    if (use_d3d9) {
        if (pSysSurface) {
            pSysSurface->Release();
            pSysSurface = nullptr;
        }
        allow_d3d9_frame = false;
    } else {
        if (!srcdc)
            return;
        SelectObject(memdc, old_bmp);
        DeleteObject(bmp);
        DeleteDC(memdc);
        ReleaseDC(hwnd, srcdc);
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
        while ((pos = cmd.find("$PROJ")) != std::string::npos)
            cmd.replace(pos, 5, cfg.project_name);
        while ((pos = cmd.find("$NAME")) != std::string::npos)
            cmd.replace(pos, 5, "output"); // TODO
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

void video::after_draw() {
    if (!recording || use_d3d9)
        return;
    RECT win_rect;
    auto ret = GetClientRect(::hwnd, &win_rect);
    ENSURE(ret);
    if (win_rect.right == 0 || win_rect.bottom == 0)
        return;
    switch (check_record({win_rect.right, win_rect.bottom})) {
    case video::CheckResult::Ok:
        break;
    case video::CheckResult::Failed:
        return;
    case video::CheckResult::Started:
        srcdc = GetDC(hwnd);
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
        spdlog::info("Window capture started");
        break;
    default:
        ASS(false);
    }
    BOOL success;
    // TODO: configure this in config?
    if (1) {
        success = BitBlt(memdc, 0, 0, size.first, size.second, srcdc, 0, 0, SRCCOPY | CAPTUREBLT);
    } else {
        success = PrintWindow(hwnd, memdc, PW_CLIENTONLY);
    }
    ENSURE(success);
    int bits = GetDIBits(memdc, bmp, 0, size.second, data_buffer.data(), &bmi, DIB_RGB_COLORS);
    ENSURE(bits == size.second);
    if (!ffmpeg.write(data_buffer.data(), data_buffer.size())) {
        spdlog::error("Failed to write data to ffmpeg");
        stop();
    }
}

void video::d3d9_draw(void* dev_ptr) {
    if (!recording || !use_d3d9 || !allow_d3d9_frame)
        return;
    allow_d3d9_frame = false;
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
        spdlog::info("Direct3D9 capture started");
        break;
    default:
        ASS(false);
    }
    ASS(data_buffer.size() == desc.Width * desc.Height * 4);
    ASS(data_buffer.size() == size.first * size.second * 4);
    D3DLOCKED_RECT lockedRect;
    d3d_ret = pSysSurface->LockRect(&lockedRect, nullptr, D3DLOCK_READONLY);
    ENSURE(d3d_ret == D3D_OK);
    if (d3d_ret == D3D_OK) {
        unsigned char* pSrc = reinterpret_cast<unsigned char*>(lockedRect.pBits);
        for (UINT y = 0; y < desc.Height; ++y)
            std::memcpy(&data_buffer[y * desc.Width * 4], pSrc + (y * lockedRect.Pitch),
                        desc.Width * 4);
        pSysSurface->UnlockRect();
    }
    pBackBuffer->Release();
    if (!ffmpeg.write(data_buffer.data(), data_buffer.size())) {
        spdlog::error("Failed to write data to ffmpeg");
        stop();
    }
}
