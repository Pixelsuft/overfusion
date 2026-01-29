#include "ui.hpp"
#include <imgui.h>

bool ui::processing;

void ui::init() {
    processing = false;
}

void ui::draw() {
    if (!ImGui::Begin("OverFusion", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }
    ImGui::Text("Created by Pixelsuft");
    ImGui::End();
}
