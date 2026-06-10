#pragma once

namespace audio {
void init();
void reinit_capture();
void flush();
bool is_recording();
} // namespace audio
