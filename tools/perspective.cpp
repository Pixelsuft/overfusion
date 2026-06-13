#include "perspective.hpp"
#include <cstring>
#include <spdlog/spdlog.h>

namespace perspective {
static void* mod_handle = nullptr;

short(__stdcall* DisplayRunObjectO)(void* pthis);
short __stdcall DisplayRunObjectH(void* pthis) { return DisplayRunObjectO(pthis); }
} // namespace perspective

void perspective::after_dll_load(ost::string_view fn, void* mod) {
    if (fn == "Perspective.mfx")
        mod_handle = mod;
    spdlog::error("LOAD {} {}", fn, mod_handle);
}

void* perspective::after_proc_get(void* module, const char* proc, void* ret) {
    if (module && module == mod_handle && std::strcmp(proc, "DisplayRunObject") == 0) {
        DisplayRunObjectO = reinterpret_cast<decltype(DisplayRunObjectO)>(ret);
        return reinterpret_cast<void*>(DisplayRunObjectH);
    }
    return ret;
}
