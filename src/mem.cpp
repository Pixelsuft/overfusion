#define WIN32_LEAN_AND_MEAN
#include "mem.hpp"
#include "ass.hpp"
#include "loadhooks.hpp"
#include "uconv.hpp"
#include <MinHook.h>
#include <Psapi.h>
#include <Windows.h>
#include <algorithm>
#include <processthreadsapi.h>
#include <spdlog/spdlog.h>
#include <tlhelp32.h>
#include <unordered_map>
#include <winternl.h>

using std::string;

namespace mem {
string exe_name;
static HMODULE base_module;
static HANDLE hproc;
} // namespace mem

namespace hook {
struct HookTarget {
    std::string funcName;
    void* hookFunc;
    void** origFunc;
};

static std::unordered_map<std::string, std::vector<HookTarget>> iat_map;
} // namespace hook

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
    auto ret = loadhooks::get_func_address(obj, func_name);
    if (ret == nullptr)
        spdlog::error("Failed to get \"{}\" address of the module \"{}\"", func_name, obj_name);
    return ret;
}

bool mem::_flush_instructions(size_t addr, size_t size) {
    auto ret = FlushInstructionCache(hproc, reinterpret_cast<void*>(addr), size);
    ENSURE(ret);
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
    ENSURE(pTarget != nullptr);
    auto mh_ret = MH_CreateHook(pTarget, pDetour, ppOriginal);
    ENSURE(mh_ret == MH_OK);
    return mh_ret == MH_OK;
}

void hook::_patch_vtable(void** vtable, int index, void* new_func, void** old_func) {
    DWORD old_protect;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect))
        spdlog::error("VirtualProtect failed while patching vtable");
    if (old_func)
        *old_func = vtable[index];
    vtable[index] = new_func;
    if (!VirtualProtect(&vtable[index], sizeof(void*), old_protect, &old_protect))
        spdlog::warn("VirtualProtect failed while patching vtable");
}

bool hook::_hook_iat(void* hModule, const char* szImportModName, const char* szFuncName,
                     void* pNewFunc, void** ppOriginal, bool by_addr) {
    ENSURE(hModule && szImportModName && szFuncName && pNewFunc);
    if (!hModule || !szImportModName || !szFuncName || !pNewFunc)
        return false;

    auto pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
    ENSURE(pDosHeader->e_magic == IMAGE_DOS_SIGNATURE);

    auto pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
        (reinterpret_cast<BYTE*>(hModule) + pDosHeader->e_lfanew));
    ENSURE(pNtHeaders->Signature == IMAGE_NT_SIGNATURE);

    auto importDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    ENSURE(importDir.VirtualAddress != 0);

    auto pImportDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
        (reinterpret_cast<BYTE*>(hModule) + importDir.VirtualAddress));

    for (; pImportDesc->Name != 0; pImportDesc++) {
        auto szModName =
            reinterpret_cast<char*>(reinterpret_cast<BYTE*>(hModule) + pImportDesc->Name);
        if (_stricmp(szModName, szImportModName) == 0) {
            DWORD thunkOffset = pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk
                                                                : pImportDesc->FirstThunk;
            if (thunkOffset == 0)
                continue;

            auto pOriginalThunk =
                reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hModule) + thunkOffset);
            auto pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hModule) +
                                                              pImportDesc->FirstThunk);

            for (; pOriginalThunk->u1.AddressOfData != 0; pOriginalThunk++, pThunk++) {
                if (by_addr || !(pOriginalThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    auto pImportByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
                        reinterpret_cast<BYTE*>(hModule) + pOriginalThunk->u1.AddressOfData);

                    if (by_addr ? reinterpret_cast<char*>(pThunk->u1.Function) == szFuncName
                                : strcmp(reinterpret_cast<char*>(pImportByName->Name),
                                         szFuncName) == 0) {
                        if (reinterpret_cast<PVOID>(pThunk->u1.Function) == pNewFunc)
                            return false;

                        DWORD dwOldProtect;
                        if (!VirtualProtect(&pThunk->u1.Function, sizeof(DWORD_PTR), PAGE_READWRITE,
                                            &dwOldProtect)) {
                            spdlog::error("VirtualProtect failed to IAT hook");
                            return false;
                        }

                        if (ppOriginal)
                            *ppOriginal = reinterpret_cast<void*>(pThunk->u1.Function);

                        pThunk->u1.Function = reinterpret_cast<DWORD_PTR>(pNewFunc);

                        if (!VirtualProtect(&pThunk->u1.Function, sizeof(DWORD_PTR), dwOldProtect,
                                            &dwOldProtect))
                            spdlog::warn("VirtualProtect failed to IAT hook");

                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool has_no_uppercase(ost::string_view sv) {
    return std::all_of(sv.begin(), sv.end(), [](unsigned char c) { return !std::isupper(c); });
}

bool hook::_reg_iat(ost::string_view dll, ost::string_view func_name, void* pNewFunc,
                    void** ppOriginal) {
    // TODO: assert for no doubling
    ASS(has_no_uppercase(dll));
    HookTarget t;
    t.funcName = func_name;
    t.hookFunc = pNewFunc;
    t.origFunc = ppOriginal;
#ifdef _DEBUG
    auto& v = iat_map[std::string(dll)];
    auto it = std::find_if(v.begin(), v.end(),
                           [t](const HookTarget& t2) { return t2.funcName == t.funcName; });
    ASS(it == v.end());
#endif
    iat_map[std::string(dll)].push_back(std::move(t));
    return true;
}

static bool module_iat_apply(void* hModule) {
    ENSURE(hModule);

    auto pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
    ENSURE(pDosHeader->e_magic == IMAGE_DOS_SIGNATURE);

    auto pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
        (reinterpret_cast<BYTE*>(hModule) + pDosHeader->e_lfanew));
    ENSURE(pNtHeaders->Signature == IMAGE_NT_SIGNATURE);

    auto importDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.VirtualAddress == 0)
        return false;

    auto pImportDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
        (reinterpret_cast<BYTE*>(hModule) + importDir.VirtualAddress));

    for (; pImportDesc->Name != 0; pImportDesc++) {
        std::string dllKey(
            reinterpret_cast<char*>(reinterpret_cast<BYTE*>(hModule) + pImportDesc->Name));
        std::transform(dllKey.begin(), dllKey.end(), dllKey.begin(), ::tolower);
        auto it = hook::iat_map.find(dllKey);
        if (it == hook::iat_map.end())
            continue;

        const auto& targets = it->second;
        DWORD thunkOffset = pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk
                                                            : pImportDesc->FirstThunk;
        if (thunkOffset == 0)
            continue;

        auto pOriginalThunk =
            reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hModule) + thunkOffset);
        auto pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hModule) +
                                                          pImportDesc->FirstThunk);

        for (; pOriginalThunk->u1.AddressOfData != 0; pOriginalThunk++, pThunk++) {
            if (!(pOriginalThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                auto pImportByName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(
                    reinterpret_cast<BYTE*>(hModule) + pOriginalThunk->u1.AddressOfData);
                auto funcName = reinterpret_cast<char*>(pImportByName->Name);
                auto target =
                    std::find_if(targets.begin(), targets.end(), [funcName](const auto& t) {
                        return strcmp(funcName, t.funcName.c_str()) == 0;
                    });
                if (target == targets.end())
                    continue;
                if (reinterpret_cast<PVOID>(pThunk->u1.Function) == target->hookFunc)
                    continue;

                DWORD dwOldProtect;
                if (!VirtualProtect(&pThunk->u1.Function, sizeof(DWORD_PTR), PAGE_READWRITE,
                                    &dwOldProtect)) {
                    spdlog::error("VirtualProtect failed to IAT hook");
                    return false;
                }

                if (target->origFunc)
                    *target->origFunc = reinterpret_cast<void*>(pThunk->u1.Function);

                pThunk->u1.Function = reinterpret_cast<DWORD_PTR>(target->hookFunc);

                if (!VirtualProtect(&pThunk->u1.Function, sizeof(DWORD_PTR), dwOldProtect,
                                    &dwOldProtect))
                    spdlog::warn("VirtualProtect failed to IAT hook");
                // spdlog::debug("{}->{} iated", dllKey, target->funcName);
            }
        }
    }
    return true;
}

bool hook::patch_iat() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to create snapshot (code: {})", GetLastError());
        return 1;
    }

    MODULEENTRY32W me = {0};
    me.dwSize = sizeof(MODULEENTRY32);

    if (Module32FirstW(hSnapshot, &me)) {
        do {
            auto mod_fn = uconv::from_utf16(me.szModule);
            std::transform(mod_fn.begin(), mod_fn.end(), mod_fn.begin(), ::tolower);
            // TODO: do not touch system modules???
            if (module_iat_apply(me.hModule)) {
                if (!mod_fn.ends_with(".sft") && !mod_fn.ends_with(".ift") &&
                    !mod_fn.ends_with(".mfx") && !mod_fn.ends_with(".mvx")) {
                    // spdlog::debug("IATed {}", mod_fn);
                }
            }
        } while (Module32NextW(hSnapshot, &me));
        CloseHandle(hSnapshot);
        return true;
    } else {
        spdlog::error("Failed to retrieve module information (code: {})", GetLastError());
        CloseHandle(hSnapshot);
        return false;
    }
}
