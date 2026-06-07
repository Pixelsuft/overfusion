#pragma once

namespace video {
void init();
void start();
void stop();
void after_draw();
void d3d9_draw(void* dev_ptr);
void set_allow_frame(bool allow);
bool is_recording();
} // namespace video
