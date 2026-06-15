#define WIN32_LEAN_AND_MEAN
#include "customwindow.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "input.hpp"
#include "ui.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <d3d9.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#pragma comment(lib, "d3d9.lib")

// TODO: separale class for windows

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);
extern IDirect3D9*(WINAPI* Direct3DCreate9O)(UINT SDKVersion);
extern HWND hwnd;

static HWND g_hwnd;
static WNDCLASSEXW g_wc;

LRESULT WINAPI CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LPDIRECT3D9 g_pD3D;
static LPDIRECT3DDEVICE9 g_pd3dDevice;
static D3DPRESENT_PARAMETERS g_d3dpp;

static HICON GetWindowIcon(HWND targetHwnd) {
    HICON hIcon = nullptr;
    hIcon = reinterpret_cast<HICON>(SendMessageW(targetHwnd, WM_GETICON, ICON_BIG, 0));
    if (hIcon == nullptr)
        hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(targetHwnd, GCLP_HICON));
    if (hIcon == nullptr)
        hIcon = reinterpret_cast<HICON>(SendMessageW(targetHwnd, WM_GETICON, ICON_SMALL2, 0));
    return hIcon;
}

static LRESULT WINAPI CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if ((msg != WM_KEYDOWN && msg != WM_KEYUP) || conf::get().show_menu)
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    switch (msg) {
    case WM_KEYDOWN:
    case WM_KEYUP: {
        // if (ImGui::GetIO().WantCaptureKeyboard && msg == WM_KEYDOWN)
        //     break;
        input::handle_input(wParam, msg == WM_KEYDOWN);
        break;
    }
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            // Reset D3D9 device with new size
            ImGui_ImplDX9_InvalidateDeviceObjects();
            HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
            if (hr == D3DERR_INVALIDCALL)
                ENSURE(false);
            ImGui_ImplDX9_CreateDeviceObjects();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU || (wParam & 0xfff0) == SC_CLOSE)
            return 0;
        break;
    case WM_CLOSE:
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

static bool RegisterCustomWindowClass(HINSTANCE hInstance) {
    ZeroMemory(&g_wc, sizeof(WNDCLASSEXW));
    g_wc.cbSize = sizeof(WNDCLASSEXW);
    g_wc.style = CS_CLASSDC;
    g_wc.lpfnWndProc = CustomWndProc;
    g_wc.hInstance = hInstance;
    g_wc.lpszClassName = L"OverFusionWindow";
    g_wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    return RegisterClassExW(&g_wc) != 0;
}

static bool CreateCustomWindow(HINSTANCE hInstance) {
    RECT rect = {0, 0, 320, 200};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowExW(0, L"OverFusionWindow", L"OverFusion", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                             rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);
    return g_hwnd != nullptr;
}

static bool InitD3D9() {
    g_pD3D = Direct3DCreate9O(D3D_SDK_VERSION);
    if (g_pD3D == nullptr) {
        spdlog::error("Failed to create D3D9 object");
        return false;
    }

    ZeroMemory(&g_d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // VSync

    RECT rect;
    GetClientRect(g_hwnd, &rect);
    g_d3dpp.BackBufferWidth = rect.right - rect.left;
    g_d3dpp.BackBufferHeight = rect.bottom - rect.top;

    HRESULT hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice);
    if (FAILED(hr)) {
        spdlog::error("Failed to create D3D9 device");
        return false;
    }
    return true;
}

static bool InitImGui() {
    if (!ui::init_imgui_context()) {
        spdlog::error("Failed to initialize ImGui context");
        return false;
    }
    ImGui::StyleColorsDark();
    if (!ui::init_imgui_platform(g_hwnd, g_pd3dDevice)) {
        spdlog::error("Failed to initialize ImGui platform/renderer");
        return false;
    }
    return true;
}

bool customwindow::init() {
    g_hwnd = nullptr;
    g_pD3D = nullptr;
    g_pd3dDevice = nullptr;
    g_d3dpp = {};
    HINSTANCE hInstance = GetModuleHandleW(nullptr);
    if (!RegisterCustomWindowClass(hInstance)) {
        spdlog::error("Failed to register custom window class");
        return false;
    }
    if (!CreateCustomWindow(hInstance)) {
        spdlog::error("Failed to create custom window");
        return false;
    }
    winhooks::fix_win32_theme(g_hwnd);
    auto hIcon = GetWindowIcon(::hwnd);
    SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
    SendMessageW(g_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
#ifdef _DEBUG
    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
#endif
    if (!InitD3D9()) {
        spdlog::error("Failed to initialize D3D9");
        return false;
    }
    if (!InitImGui()) {
        spdlog::error("Failed to initialize ImGui");
        return false;
    }
    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    // SetParent(g_hwnd, hwnd);

    spdlog::info("Custom window initialized successfully");
    return true;
}

void customwindow::cleanup() {
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
    if (g_pD3D) {
        g_pD3D->Release();
        g_pD3D = nullptr;
    }

    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }

    UnregisterClassW(L"OverFusionWindow", GetModuleHandleW(nullptr));

    spdlog::debug("Custom window cleaned up");
}

void customwindow::render() {
    ui::set_processing(true);
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ui::draw();
    ImGui::EndFrame();
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    D3DCOLOR clear_color = D3DCOLOR_RGBA(0, 0, 0, 255);
    g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_color, 1.0f, 0);
    if (g_pd3dDevice->BeginScene() >= 0) {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        g_pd3dDevice->EndScene();
    }
    HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
    if (result == D3DERR_DEVICELOST &&
        g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        g_pd3dDevice->Reset(&g_d3dpp);
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    ui::set_processing(false);
}
