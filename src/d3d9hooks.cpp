#define WIN32_LEAN_AND_MEAN
#include "d3d9hooks.hpp"
#include "ass.hpp"
#include "config.hpp"

#include "mem.hpp"
#include "ui.hpp"
#include <Windows.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <d3d9.h>
#include <spdlog/spdlog.h>

static long(__stdcall* ResetO)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*) = nullptr;

static long(__stdcall* EndSceneO)(LPDIRECT3DDEVICE9) = nullptr;

static HRESULT(WINAPI* CreateDeviceO)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
                                      D3DPRESENT_PARAMETERS*, IDirect3DDevice9**) = nullptr;

extern HWND hwnd;

static HRESULT(WINAPI* DirectDrawCreateO)(void* lpGUID, void** lplpDD, void* pUnkOuter);
static HRESULT WINAPI DirectDrawCreateH(void* lpGUID, void** lplpDD, void* pUnkOuter) {
    spdlog::warn("The game is using DirectDraw, forcing custom window");
    return DirectDrawCreateO(lpGUID, lplpDD, pUnkOuter);
}

static long __stdcall ResetH(LPDIRECT3DDEVICE9 pDevice,
                             D3DPRESENT_PARAMETERS* pPresentationParameters) {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    long result = ResetO(pDevice, pPresentationParameters);
    ImGui_ImplDX9_CreateDeviceObjects();
    return result;
}

static long __stdcall EndSceneH(LPDIRECT3DDEVICE9 pDevice) {
    ui::processing = true;
    static bool inited = false;
    if (!inited) {
        inited = true;
        // Check if we're using custom window
        if (conf::get().custom_window) {
            // Custom window is handled separately, don't initialize here
            // ImGui for the game's D3D9 device
            ui::processing = false;
            auto ret = EndSceneO(pDevice);
            return ret;
        }

        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);
        ASS(::hwnd == params.hFocusWindow);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.IniFilename = nullptr;
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX9_Init(pDevice);
    }
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ui::draw();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    ui::processing = false;
    auto ret = EndSceneO(pDevice);
    return ret;
}

static HRESULT WINAPI CreateDeviceH(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType,
                                    HWND hFocusWindow, DWORD BehaviorFlags,
                                    D3DPRESENT_PARAMETERS* pPresentationParameters,
                                    IDirect3DDevice9** ppReturnedDeviceInterface) {
    spdlog::debug("IDirect3D9->CreateDevice");
    HRESULT hr = CreateDeviceO(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                               pPresentationParameters, ppReturnedDeviceInterface);

    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface) {
        IDirect3DDevice9* pDevice = *ppReturnedDeviceInterface;
        void** vtable = *(void***)pDevice;
        // Should be called once, right?
        hook::hook(vtable[16], ResetH, &ResetO);
        hook::hook(vtable[42], EndSceneH, &EndSceneO);
        hook::enable();
    }
    return hr;
}

static IDirect3D9*(WINAPI* Direct3DCreate9O)(UINT SDKVersion);
static IDirect3D9* WINAPI Direct3DCreate9H(UINT SDKVersion) {
    spdlog::debug("Direct3DCreate9");
    auto ret = Direct3DCreate9O(SDKVersion);
    if (SUCCEEDED(ret) && !CreateDeviceO) {
        void** vtable = *(void***)ret;
        hook::patch_vtable(vtable, 16, CreateDeviceH, &CreateDeviceO);
    }
    return ret;
}

void d3d9hooks::pre_init() { HOOK_AUTO("ddraw.dll", DirectDrawCreate); }

void d3d9hooks::init() { HOOK_AUTO("d3d9.dll", Direct3DCreate9); }
