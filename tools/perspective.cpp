#include "perspective.hpp"
#include "../src/config.hpp"
#include "../src/log.hpp"
#include <cstring>
#include <imgui.h>

namespace perspective {
static void* mod_handle = nullptr;

static short(__stdcall* DisplayRunObjectO)(void* pthis);
static short __stdcall DisplayRunObjectH(void* pthis) {
    if (conf::get().disable_perspective)
        return 0;
    return DisplayRunObjectO(pthis);
}
} // namespace perspective

void perspective::after_dll_load(of::string_view fn, void* mod) {
    if (!mod_handle && fn == "Perspective.mfx")
        mod_handle = mod;
}

void* perspective::after_proc_get(void* module, const char* proc, void* ret) {
    if (module && module == mod_handle && std::strcmp(proc, "DisplayRunObject") == 0) {
        DisplayRunObjectO = reinterpret_cast<decltype(DisplayRunObjectO)>(ret);
        return reinterpret_cast<void*>(DisplayRunObjectH);
    }
    return ret;
}

void perspective::draw_menu() {
    if (mod_handle == nullptr)
        return;
    auto& cfg = conf::get();
    ImGui::Checkbox("Disable Perspective.mfx", &cfg.disable_perspective);
}
