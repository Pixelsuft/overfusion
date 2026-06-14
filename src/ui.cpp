#define WIN32_LEAN_AND_MEAN
#include "ui.hpp"
#include "audio.hpp"
#include "config.hpp"
#include "files.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "video.hpp"
#include <Windows.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <d3d9.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#undef min
#undef max

constexpr bool ui_save_sets = true;

namespace ui {
static bool processing;
}

void ui::init() { processing = false; }

void ui::set_processing(bool enabled) { processing = enabled; }

bool ui::is_processing() { return processing; }

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

bool ui::init_imgui_platform(void* hwnd, void* device) {
    processing = true;
    if (!ImGui_ImplWin32_Init(reinterpret_cast<HWND>(hwnd))) {
        spdlog::error("Failed to initialize ImGui Win32 impl");
        processing = false;
        return false;
    }
    if (!ImGui_ImplDX9_Init(reinterpret_cast<LPDIRECT3DDEVICE9>(device))) {
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
    if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Replay mode", &cfg.is_replay))
            state::on_mode_switch();
        ImGui::Checkbox("Reset on replay", &cfg.reset_on_replay);
        ImGui::Checkbox("Paused", &cfg.is_paused);
        ImGui::Checkbox("Fast forward", &cfg.fast_forward);
        if (ImGui::SliderFloat("Speed", &cfg.speed, 0.05f, 2.f))
            cfg.speed = std::min(std::max(cfg.speed, 0.05f), 2.f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            cfg.speed = 1.f;
        ImGui::Checkbox("Show info window", &cfg.show_info);
        static char replay_buf[1024] = "replay.ofr";
        ImGui::InputText("Replay filename", replay_buf, 1024);
        if (ImGui::Button("Export"))
            state::export_replay(replay_buf);
        if (ImGui::Button("Import"))
            state::import_replay(replay_buf);
        if (ImGui::Button("Clear temp events queue"))
            state::clear_temp_events();
    }
    if (ImGui::Button("Restart"))
        state::reset_game();
    if (cfg.virtual_fs && ImGui::CollapsingHeader("Virtual Filesystem")) {
        files::draw_ui();
    }
    if (ImGui::CollapsingHeader("Recording")) {
        if (!video::is_recording()) {
            if (ImGui::Button("Start video recording"))
                video::start();
        } else {
            if (ImGui::Button("Stop video recording"))
                video::stop();
        }
        if (cfg.record_audio && audio::is_recording()) {
            if (ImGui::Button("Stop audio capture"))
                audio::flush();
        }
    }
    if (ImGui::CollapsingHeader("Plugin")) {
        plug::get().draw_menu();
    }
    if (ImGui::CollapsingHeader("About")) {
        ImGui::Text("Created by Pixelsuft");
    }
#ifdef _DEBUG
    if (ImGui::CollapsingHeader("Debug")) {
        static int frame_id = 0;
        ImGui::InputInt("Next frame id", &frame_id);
        if (ImGui::Button("Switch")) {
            void* pState = plug::get().get_prop(plug::PtrProp::PState);
            short* ptr = reinterpret_cast<short*>(
                plug::get().get_prop(plug::PtrProp::PNextFrameTask, pState));
            *ptr = 3;
            ptr = reinterpret_cast<short*>(
                plug::get().get_prop(plug::PtrProp::PNextFrameData, pState));
            *ptr = static_cast<short>(frame_id) | 0x8000;
            spdlog::info("doing...");
        }
    }
#endif
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
    if (!cfg.custom_window && cfg.draw_cursor) {
        auto m_pos = state::get_tas_mouse_pos();
        auto m_down = state::get_tas_mouse_down(VK_LBUTTON);
        if (m_pos.first >= 0.f && m_pos.second >= 0.f) {
            auto w_pos = plug::get().mouse_to_window(m_pos.first, m_pos.second);
            ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
            draw_list->AddCircleFilled(
                ImVec2(static_cast<float>(w_pos.first), static_cast<float>(w_pos.second)), 3.f,
                m_down ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255), 8);
        }
    }
}
