#define WIN32_LEAN_AND_MEAN
#include "customwindow.hpp"
#include "ass.hpp"
#include "config.hpp"
#include "input.hpp"
#include "log.hpp"
#include "ui.hpp"
#include "winhooks.hpp"
#include <Windows.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <d3d9.h>
#include <imgui.h>
#pragma comment(lib, "d3d9.lib")

extern HWND(WINAPI* CreateWindowExWO)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND,
                                      HMENU, HINSTANCE, LPVOID);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);
extern IDirect3D9*(WINAPI* Direct3DCreate9O)(UINT SDKVersion);
extern HWND hwnd;

namespace customwindow {
class Window {
    WNDCLASSEXW m_wc;
    LPDIRECT3D9 m_pD3D;
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    D3DPRESENT_PARAMETERS m_d3dpp;
    HWND m_hwnd;
    const wchar_t* class_name;
    ImGuiContext* ctx;

    bool RegisterCustomWindowClass(HINSTANCE hInstance);
    bool CreateCustomWindow(HINSTANCE hInstance);
    bool InitD3D9();
    bool InitImGui();
    static LRESULT WINAPI CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static HICON GetWindowIcon(HWND targetHwnd);

public:
    Window(const wchar_t* cls_name);
    ~Window();
    bool init();
    void set_shown(bool show);
    void cleanup();
    void render();
    HWND get_handle();
};

static Window* g_info;
static Window* g_menu;
} // namespace customwindow

using customwindow::Window;

Window::Window(const wchar_t* cls_name) {
    class_name = cls_name;
    m_hwnd = nullptr;
    m_wc = {};
    m_pD3D = nullptr;
    m_pd3dDevice = nullptr;
    m_d3dpp = {};
}

Window::~Window() {}

HWND Window::get_handle() { return m_hwnd; }

HICON Window::GetWindowIcon(HWND targetHwnd) {
    HICON hIcon = nullptr;
    hIcon = reinterpret_cast<HICON>(SendMessageW(targetHwnd, WM_GETICON, ICON_BIG, 0));
    if (hIcon == nullptr)
        hIcon = reinterpret_cast<HICON>(GetClassLongPtrW(targetHwnd, GCLP_HICON));
    if (hIcon == nullptr)
        hIcon = reinterpret_cast<HICON>(SendMessageW(targetHwnd, WM_GETICON, ICON_SMALL2, 0));
    return hIcon;
}

LRESULT WINAPI Window::CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* self = g_info->get_handle() == hWnd
                       ? g_info
                       : ((g_menu && g_menu->get_handle() == hWnd) ? g_menu : nullptr);
    if (!self)
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);

    if ((msg != WM_KEYDOWN && msg != WM_KEYUP) || conf::get().show_menu) {
        ImGui::SetCurrentContext(self->ctx);
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_KEYDOWN:
    case WM_KEYUP:
        // ImGui::SetCurrentContext(self->ctx);
        input::handle_input(wParam, msg == WM_KEYDOWN);
        break;
    case WM_SIZE:
        if (self->m_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            ImGui::SetCurrentContext(self->ctx);
            self->m_d3dpp.BackBufferWidth = LOWORD(lParam);
            self->m_d3dpp.BackBufferHeight = HIWORD(lParam);
            ImGui_ImplDX9_InvalidateDeviceObjects();
            HRESULT hr = self->m_pd3dDevice->Reset(&self->m_d3dpp);
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

bool Window::RegisterCustomWindowClass(HINSTANCE hInstance) {
    ZeroMemory(&m_wc, sizeof(WNDCLASSEXW));
    m_wc.cbSize = sizeof(WNDCLASSEXW);
    m_wc.style = CS_CLASSDC;
    m_wc.lpfnWndProc = CustomWndProc;
    m_wc.hInstance = hInstance;
    m_wc.lpszClassName = class_name;
    m_wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    return RegisterClassExW(&m_wc) != 0;
}

bool Window::CreateCustomWindow(HINSTANCE hInstance) {
    RECT rect = {0, 0, 320, 200};
    if (this == g_menu)
        rect = {0, 0, 400, 600};
    DWORD style = WS_OVERLAPPED | WS_CAPTION |
                  (this == g_menu ? 0 : (WS_MINIMIZEBOX | WS_SYSMENU)) | WS_THICKFRAME;
    AdjustWindowRect(&rect, style, FALSE);
    m_hwnd =
        CreateWindowExWO(0, class_name, this == g_menu ? L"OverFusion Menu" : L"OverFusion Info",
                         style, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                         rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);
    if (m_hwnd)
        winhooks::fix_win32_theme_instant(m_hwnd);
    return m_hwnd != nullptr;
}

bool Window::InitD3D9() {
    m_pD3D = Direct3DCreate9O(D3D_SDK_VERSION);
    if (m_pD3D == nullptr) {
        of::error("Failed to create D3D9 object");
        return false;
    }

    ZeroMemory(&m_d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    m_d3dpp.Windowed = TRUE;
    m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    m_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    m_d3dpp.EnableAutoDepthStencil = TRUE;
    m_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    RECT rect;
    GetClientRect(m_hwnd, &rect);
    m_d3dpp.BackBufferWidth = rect.right - rect.left;
    m_d3dpp.BackBufferHeight = rect.bottom - rect.top;

    HRESULT hr = m_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hwnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING, &m_d3dpp, &m_pd3dDevice);
    if (FAILED(hr)) {
        of::error("Failed to create D3D9 device");
        return false;
    }
    return true;
}

bool Window::InitImGui() {
    ctx = reinterpret_cast<ImGuiContext*>(ui::init_imgui_context());
    if (!ctx) {
        of::error("Failed to initialize ImGui context");
        return false;
    }
    ImGui::SetCurrentContext(ctx);
    ImGui::StyleColorsDark();
    if (!ui::init_imgui_platform(m_hwnd, m_pd3dDevice)) {
        of::error("Failed to initialize ImGui platform/renderer");
        return false;
    }
    return true;
}

bool Window::init() {
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    if (!RegisterCustomWindowClass(hInstance)) {
        of::error("Failed to register custom window class");
        return false;
    }
    if (!CreateCustomWindow(hInstance)) {
        of::error("Failed to create custom window");
        return false;
    }

    winhooks::fix_win32_theme(m_hwnd);
    auto hIcon = GetWindowIcon(::hwnd);
    SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
    SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));

    if (!InitD3D9()) {
        of::error("Failed to initialize D3D9");
        return false;
    }
    if (!InitImGui()) {
        of::error("Failed to initialize ImGui");
        return false;
    }

    UpdateWindow(m_hwnd);
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    return true;
}

void Window::cleanup() {
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (m_pd3dDevice) {
        m_pd3dDevice->Release();
        m_pd3dDevice = nullptr;
    }
    if (m_pD3D) {
        m_pD3D->Release();
        m_pD3D = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClassW(class_name, GetModuleHandleW(nullptr));
}

void Window::render() {
    ImGui::SetCurrentContext(ctx);
    ui::set_processing(true);
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ui::draw(this == g_menu);
    ImGui::EndFrame();

    m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    D3DCOLOR clear_color = D3DCOLOR_RGBA(0, 0, 0, 255);
    m_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_color, 1.0f, 0);

    if (m_pd3dDevice->BeginScene() >= 0) {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        m_pd3dDevice->EndScene();
    }

    HRESULT result = m_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
    if (result == D3DERR_DEVICELOST &&
        m_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        m_pd3dDevice->Reset(&m_d3dpp);
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    ui::set_processing(false);
}

void Window::set_shown(bool show) { ShowWindow(m_hwnd, show ? SW_SHOW : SW_HIDE); }

bool customwindow::init() {
    auto& cfg = conf::get();
    ASS(g_info == nullptr);
    ASS(g_menu == nullptr);
    g_menu = nullptr;
    g_info = new Window(L"OverFusionInfo");
    auto ret = g_info->init();
    ENSURE(ret);
    g_menu = new Window(L"OverFusionMenu");
    ret = g_menu->init();
    ENSURE(ret);
    if (cfg.show_info)
        g_info->set_shown(true);
    if (cfg.show_menu)
        g_menu->set_shown(true);
    return true;
}

void customwindow::cleanup() {
    if (g_menu) {
        g_menu->cleanup();
        delete g_menu;
    }
    if (g_info) {
        g_info->cleanup();
        delete g_info;
    }
}

void customwindow::render() {
    auto& cfg = conf::get();
    if (g_info && cfg.show_info)
        g_info->render();
    if (g_menu && cfg.show_menu)
        g_menu->render();
}

void customwindow::update_menu_show() {
    winhooks::fix_win32_sys_anim(g_menu->get_handle(), true);
    g_menu->set_shown(conf::get().show_menu);
    winhooks::fix_win32_sys_anim(g_menu->get_handle(), false);
}
