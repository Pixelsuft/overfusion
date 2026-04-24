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
static bool use_d3d9; // TODO
static bool recording;

// Win32 recording
static BITMAPINFO bmi;
static HDC srcdc;
static HDC memdc;
static HBITMAP bmp;
static HGDIOBJ old_bmp;
} // namespace video

void video::init() {
    srcdc = nullptr;
    use_d3d9 = false;
    recording = false;
    size.first = size.second = 0;
}

void video::start() {
    if (recording)
        return;
    data_buffer.clear();
    auto& cfg = conf::get();
    recording = true;
    use_d3d9 = false;
}

void video::stop() {
    if (!recording)
        return;
    spdlog::info("Stopping video recording");
    ffmpeg.close();
    recording = false;
    if (use_d3d9) {

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
    if (!recording)
        return;
    RECT win_rect;
    auto ret = GetClientRect(::hwnd, &win_rect);
    ENSURE(ret);
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
        break;
    default:
        ASS(false);
    }
    BOOL success;
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
