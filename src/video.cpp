#define WIN32_LEAN_AND_MEAN
#include "video.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "subprocess.hpp"
#include <d3d9.h>
#include <spdlog/spdlog.h>

using std::string;

extern HWND hwnd;

namespace video {
static subprocess::Process ffmpeg;
static std::pair<int, int> size;
static bool use_d3d9;
static bool recording;
} // namespace video

void video::init() {
    use_d3d9 = false;
    recording = false;
    size.first = size.second = 0;
}

void video::start() {
    if (recording)
        return;
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
}

static bool check_record(std::pair<int, int> new_size) {
    auto& cfg = conf::get();
    if (video::recording && video::ffmpeg.is_open() && video::size != new_size) {
        spdlog::warn("Window resized, writing to another video file");
        video::size = new_size;
        auto ret = video::ffmpeg.close();
        ENSURE(ret);
        if (!ret)
            return false;
    }
    if (video::recording && !video::ffmpeg.is_open()) {
        string cmd = cfg.ffmpeg_cmdline;
        if (size_t pos = cmd.find("$FPS"); pos != std::string::npos)
            cmd.replace(pos, 4, std::to_string(cfg.fps));
        if (size_t pos = cmd.find("$SIZE"); pos != std::string::npos)
            cmd.replace(pos, 5,
                        std::to_string(new_size.first) + "x" + std::to_string(new_size.second));
        if (size_t pos = cmd.find("$NAME"); pos != std::string::npos)
            cmd.replace(pos, 5, "output"); // TODO
        spdlog::info("running: {}", cmd);
        if (!video::ffmpeg.open(cmd)) {
            video::recording = false;
            spdlog::error("Failed to start ffmpeg, stopping recording");
            return false;
        }
    }
    return true;
}

void video::after_draw() {
    if (!recording)
        return;
    RECT win_rect;
    auto ret = GetClientRect(::hwnd, &win_rect);
    ENSURE(ret);
    if (!check_record({win_rect.right, win_rect.bottom}))
        return;
}
