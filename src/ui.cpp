#include "ui.hpp"
#include "config.hpp"
#include "state.hpp"
#include <imgui.h>

constexpr bool ui_save_sets = true;

bool ui::processing;

void ui::init() { processing = false; }

static void draw_info() {
    if (!ImGui::Begin("OverFusion Info", nullptr,
                      (ui_save_sets ? 0 : ImGuiWindowFlags_NoSavedSettings) |
                          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return;
    }
    state::draw_info();
    ImGui::End();
}

static void draw_menu() {
    if (!ImGui::Begin("OverFusion", nullptr,
                      (ui_save_sets ? 0: ImGuiWindowFlags_NoSavedSettings))) {
        ImGui::End();
        return;
    }
    ImGui::Text("Created by Pixelsuft");
    ImGui::End();
}

void ui::draw() {
    auto& cfg = conf::get();
    if (cfg.show_info) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        draw_info();
        ImGui::PopStyleVar();
    }
    if (cfg.show_menu)
        draw_menu();
}
