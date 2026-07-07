#pragma once

namespace audio {
void init();
void reinit_capture();
void flush();
void finish();
bool is_recording();
} // namespace audio
