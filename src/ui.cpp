#include "ui.hpp"
#include "config.hpp"
#include <imgui.h>

bool ui::processing;

void ui::init() { processing = false; }

static void draw_info() {
    if (!ImGui::Begin("OverFusion Info", nullptr,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar |
                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return;
    }
    ImGui::Text("TODO");
    ImGui::End();
}

static void draw_menu() {
    if (!ImGui::Begin("OverFusion", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
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
