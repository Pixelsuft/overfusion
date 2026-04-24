#pragma once

namespace video {
void init();
void start();
void stop();
void after_draw();
void d3d9_draw(void* dev_ptr);
void set_allow_d3d9_frame(bool allow);
}
