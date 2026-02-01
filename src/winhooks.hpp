#pragma once

namespace winhooks {
    bool fix_win32_theme();
    void init();
    void after_ui_init();
    void sim_key_event(int vk, bool down);
}
