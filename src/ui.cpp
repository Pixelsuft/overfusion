#define WIN32_LEAN_AND_MEAN
#include "ui.hpp"
#include "audio.hpp"
#include "config.hpp"
#include "files.hpp"
#include "log.hpp"
#include "plugbase.hpp"
#include "state.hpp"
#include "video.hpp"
#include <Windows.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>
#include <d3d9.h>
#include <imgui.h>
#undef min
#undef max

namespace ui {
constexpr bool save_sets = true;
static bool processing;
} // namespace ui

void ui::init() { processing = false; }

void ui::set_processing(bool enabled) { processing = enabled; }

bool ui::is_processing() { return processing; }

void* ui::init_imgui_context() {
    processing = true;
    IMGUI_CHECKVERSION();
    auto ret = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    processing = false;
    return ret;
}

bool ui::init_imgui_platform(void* hwnd, void* device) {
    processing = true;
    if (!ImGui_ImplWin32_Init(reinterpret_cast<HWND>(hwnd))) {
        of::error("Failed to initialize ImGui Win32 impl");
        processing = false;
        return false;
    }
    if (!ImGui_ImplDX9_Init(reinterpret_cast<LPDIRECT3DDEVICE9>(device))) {
        of::error("Failed to initialize ImGui DX9 impl");
        processing = false;
        return false;
    }
    processing = false;
    return true;
}

static void draw_info(bool custom_window) {
    ImGuiWindowFlags flags = (ui::save_sets ? 0 : ImGuiWindowFlags_NoSavedSettings) |
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

static void draw_menu(bool custom_window) {
    auto& cfg = conf::get();
    ImGui::SetNextWindowFocus();
    ImGuiWindowFlags flags =
        (ui::save_sets ? 0 : ImGuiWindowFlags_NoSavedSettings) |
        (custom_window
             ? (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)
             : 0);
    if (!ImGui::Begin("OverFusion", nullptr, flags)) {
        ImGui::End();
        return;
    }
    if (custom_window) {
        ImGui::SetWindowPos(ImVec2(0, 0));
        auto& io = ImGui::GetIO();
        ImGui::SetWindowSize(io.DisplaySize);
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
        static char replay_buf[1024] = "replay";
        ImGui::InputText("Replay filename", replay_buf, 1024);
        if (ImGui::Button("Export"))
            state::export_replay(replay_buf);
        ImGui::SameLine();
        if (ImGui::Button("Import"))
            state::import_replay(replay_buf);
        if (ImGui::Button("Restart"))
            state::reset_game();
        if (ImGui::Button("Clear temp events queue"))
            state::clear_temp_events();
    }
    if (ImGui::CollapsingHeader("Plugin")) {
        plug::get().draw_menu();
    }
    if (ImGui::CollapsingHeader("Random")) {
        state::draw_random_tab();
    }
    if (ImGui::CollapsingHeader("Recording")) {
        if (!video::is_recording()) {
            if (ImGui::Button("Start video recording"))
                video::start();
        } else {
            if (ImGui::Button("Stop video recording"))
                video::stop();
        }
        if (cfg.allow_audio_hook && !cfg.disable_audio && !audio::is_recording() &&
            state::get_frame_counter() == 0) {
            if (ImGui::Button("Start audio capture")) {
                cfg.record_audio = true;
                audio::reinit_capture();
            }
        }
        if (cfg.record_audio && audio::is_recording()) {
            if (ImGui::Button("Stop audio capture"))
                audio::finish();
        }
    }
    if (cfg.virtual_fs && ImGui::CollapsingHeader("Virtual Filesystem")) {
        files::draw_ui();
    }
    if (ImGui::CollapsingHeader("Settings")) {
        if (ImGui::SliderFloat("Font scale", &cfg.font_scale, 0.05f, 3.f))
            cfg.font_scale = std::min(std::max(cfg.font_scale, 0.05f), 3.f);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            cfg.font_scale = 1.f;
        ImGui::Checkbox("Allow timer fix", &cfg.allow_timers_fix);
        ImGui::Checkbox("Draw cursor", &cfg.draw_cursor);
        ImGui::Checkbox("Pause on scene switch", &cfg.pause_on_scene_switch);
        ImGui::Checkbox("Redraw on frame drawing skip", &cfg.redraw_on_skip);
        ImGui::Checkbox("Save game state", &cfg.save_game_state);
        ImGui::Checkbox("Save VFS in state", &cfg.save_vfs);
        if (cfg.render_type == conf::RenderType::D3D9) {
            ImGui::Checkbox("Show info window", &cfg.show_info);
            ImGui::Checkbox("Pixel filter", &cfg.pixel_filter);
        }
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
            of::info("doing...");
        }
    }
#endif
    if (ImGui::CollapsingHeader("About and Help")) {
        ImGui::Text("Created by Pixelsuft");
        ImGui::Text("Plugin: %s", plug::get().name.c_str());
    }
    ImGui::End();
}

void ui::draw(bool force_custom_menu) {
    auto& cfg = conf::get();
    ImGui::GetIO().FontGlobalScale = cfg.font_scale;
    if (cfg.show_info && !force_custom_menu) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        draw_info(cfg.custom_window);
        ImGui::PopStyleVar();
    }
    if (cfg.show_menu && !cfg.custom_window)
        draw_menu(false);
    if (force_custom_menu) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        draw_menu(true);
        ImGui::PopStyleVar();
    }
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
