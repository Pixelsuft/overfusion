#include "ui.hpp"
#include "config.hpp"
#include "files.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

constexpr bool ui_save_sets = true;

bool ui::processing;

void ui::init() { processing = false; }

bool ui::init_imgui_context() {
    processing = true;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    processing = false;
    return true;
}

bool ui::init_imgui_platform(HWND hwnd, LPDIRECT3DDEVICE9 device) {
    processing = true;
    if (!ImGui_ImplWin32_Init(hwnd)) {
        spdlog::error("Failed to initialize ImGui Win32 impl");
        processing = false;
        return false;
    }
    if (!ImGui_ImplDX9_Init(device)) {
        spdlog::error("Failed to initialize ImGui DX9 impl");
        processing = false;
        return false;
    }
    processing = false;
    return true;
}

static void draw_info(bool custom_window) {
    ImGuiWindowFlags flags = (ui_save_sets ? 0 : ImGuiWindowFlags_NoSavedSettings) |
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar;
    if (custom_window) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    }
    if (!ImGui::Begin("OverFusion Info", nullptr, flags)) {
        ImGui::End();
        return;
    }
    if (custom_window) {
        ImGui::SetWindowPos(ImVec2(0, 0));
        auto& io = ImGui::GetIO();
        ImGui::SetWindowSize(io.DisplaySize);
    }
    state::draw_info();
    ImGui::End();
}

static void draw_menu() {
    auto& cfg = conf::get();
    ImGui::SetNextWindowFocus();
    if (!ImGui::Begin("OverFusion", nullptr,
                      (ui_save_sets ? 0 : ImGuiWindowFlags_NoSavedSettings))) {
        ImGui::End();
        return;
    }
    if (cfg.virtual_fs && ImGui::CollapsingHeader("Virtual Filesystem")) {
        files::draw_ui();
    }
    if (ImGui::CollapsingHeader("About")) {
        ImGui::Text("Created by Pixelsuft");
    }
    static int frame_id = 0;
    ImGui::InputInt("Next frame id", &frame_id);
    if (ImGui::Button("Switch")) {
        void* pState = plug::get().get_prop(plug::PtrProp::PState);
        short* ptr =
            reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameTask, pState));
        *ptr = 3;
        ptr = reinterpret_cast<short*>(plug::get().get_prop(plug::PtrProp::PNextFrameData, pState));
        *ptr = static_cast<short>(frame_id) | 0x8000;
        spdlog::info("doing...");
    }
    ImGui::End();
}

void ui::draw() {
    auto& cfg = conf::get();
    if (cfg.show_info) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        draw_info(cfg.custom_window);
        ImGui::PopStyleVar();
    }
    if (cfg.show_menu)
        draw_menu();
}
