#include "SettingsGui.h"

#include <glm/gtc/type_ptr.hpp>

#include "../imgui/ImGui.h"

void SettingsGui::draw(Settings &settings) {
    using namespace ImGui;
    SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    SetNextWindowSize(ImVec2(300, 550), ImGuiCond_FirstUseEver);
    Begin("Settings", nullptr, 0);

    if (CollapsingHeader("Sun")) {
        PushID("sun");
        BeginTable("dir_input", 2);
        TableNextColumn();
        SliderFloat("Az", &settings.sun.azimuth, 0, 360, "%.1f °");
        TableNextColumn();
        SliderFloat("El", &settings.sun.elevation, -90, 90, "%.1f °");
        EndTable();
        ColorEdit3("Color", glm::value_ptr(settings.sun.color), ImGuiColorEditFlags_Float);
        SliderFloat("Power", &settings.sun.power, 0, 50);
        PopID();
    }

    End();
}
