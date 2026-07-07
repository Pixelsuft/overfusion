#include "viewport.hpp"
#include "../src/config.hpp"
#include <cstring>
#include <imgui.h>

namespace viewport {
static void* mod_handle = nullptr;

short(__stdcall* DisplayRunObjectO)(void* pthis);
short __stdcall DisplayRunObjectH(void* pthis) {
    if (conf::get().disable_viewport)
        return 0;
    return DisplayRunObjectO(pthis);
}
} // namespace viewport

void viewport::after_dll_load(of::string_view fn, void* mod) {
    if (!mod_handle && fn == "Viewport.mfx")
        mod_handle = mod;
}

void* viewport::after_proc_get(void* module, const char* proc, void* ret) {
    if (module && module == mod_handle && std::strcmp(proc, "DisplayRunObject") == 0) {
        DisplayRunObjectO = reinterpret_cast<decltype(DisplayRunObjectO)>(ret);
        return reinterpret_cast<void*>(DisplayRunObjectH);
    }
    return ret;
}

void viewport::draw_menu() {
    if (mod_handle == nullptr)
        return;
    auto& cfg = conf::get();
    ImGui::Checkbox("Disable Viewport.mfx", &cfg.disable_viewport);
}
