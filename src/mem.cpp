#define WIN32_LEAN_AND_MEAN
#include "mem.hpp"
#include "ass.hpp"
#include "loadhooks.hpp"
#include "uconv.hpp"
#include <MinHook.h>
#include <Psapi.h>
#include <Windows.h>
#include <processthreadsapi.h>
#include <spdlog/spdlog.h>

using std::string;

namespace mem {
string exe_name;
static HMODULE base_module;
static HANDLE hproc;
} // namespace mem

void mem::init() {
    base_module = GetModuleHandleW(nullptr);
    hproc = GetCurrentProcess();
    auto mh_ret = MH_Initialize();
    ENSURE(base_module != nullptr);
    ENSURE(hproc != nullptr);
    ENSURE(mh_ret == MH_OK);
    [] {
        wchar_t buf[MAX_PATH];
        auto ret_len = GetModuleBaseNameW(hproc, base_module, buf, MAX_PATH);
        ENSURE(ret_len > 0);
        buf[ret_len] = L'\0';
        exe_name = uconv::from_utf16(buf);
    }();
}

void mem::terminate() {
    std::exit(0);
    TerminateProcess(hproc, 1);
}

size_t mem::get_base() { return reinterpret_cast<size_t>(base_module); }

size_t mem::get_base(const char* obj_name) {
    // Hopefully nobody will use unicode
    auto ret = reinterpret_cast<size_t>(GetModuleHandleA(obj_name));
    if (ret == 0)
        spdlog::error("Failed to get base address of the module \"{}\"", obj_name);
    return ret;
}

void* mem::get_addr(const char* obj_name, const char* func_name) {
    auto obj = GetModuleHandleA(obj_name);
    if (obj == nullptr) {
        spdlog::error("Failed to get module \"{}\"", obj_name);
        return nullptr;
    }
    auto ret = loadhook::get_func_address(obj, func_name);
    if (ret == nullptr)
        spdlog::error("Failed to get \"{}\" address of the module \"{}\"", func_name, obj_name);
    return ret;
}

bool mem::_write_memory(size_t addr, const void* data, size_t size) {
    SIZE_T bytesWritten;
    auto ret = WriteProcessMemory(hproc, reinterpret_cast<void*>(addr), data, size, &bytesWritten);
    ENSURE(ret && bytesWritten == size);
    return ret && bytesWritten == size;
}

bool hook::_enable_target(void* target) {
    auto mh_ret = MH_EnableHook(target);
    ENSURE(mh_ret == MH_OK);
    return mh_ret == MH_OK;
}

bool hook::_hook_target(void* pTarget, void* pDetour, void** ppOriginal) {
    auto mh_ret = MH_CreateHook(pTarget, pDetour, ppOriginal);
    ENSURE(mh_ret == MH_OK);
    return mh_ret == MH_OK;
}

void hook::_patch_vtable(void** vtable, int index, void* new_func, void** old_func) {
    DWORD old_protect;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect))
        spdlog::error("VirtualProtect failed");
    if (old_func)
        *old_func = vtable[index];
    vtable[index] = new_func;
    if (!VirtualProtect(&vtable[index], sizeof(void*), old_protect, &old_protect))
        spdlog::error("VirtualProtect failed");
}
