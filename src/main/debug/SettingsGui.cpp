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
    if (CollapsingHeader("Shadow")) {
        PushID("shadow");
        DragFloat("Dimension", &settings.shadow.dimension);
        DragFloat("Start", &settings.shadow.start);
        DragFloat("End", &settings.shadow.end);
        SliderFloat("Extrusion Bias", &settings.shadow.extrusionBias, -10, 10);
        DragFloat("Normal Bias", &settings.shadow.normalBias);
        SliderFloat("Sample Bias", &settings.shadow.sampleBias, 0.0f, 10.0f);
        SliderFloat("Sample Bias Clamp", &settings.shadow.sampleBiasClamp, 0.0f, 1.0f, "%.5f");
        DragFloat("Depth Bias Const", &settings.shadow.depthBiasConstant);
        SliderFloat("Depth Bias Slope", &settings.shadow.depthBiasSlope, -2.5f, 2.5f, "%.5f");
        SliderFloat("Depth Bias Clamp", &settings.shadow.depthBiasClamp, 0.0f, 0.1f, "%.5f");
        PopID();
    }
    if (CollapsingHeader("Tonemap")) {
        PushID("tonemap");
        DragFloat("EV Min", &settings.agx.ev_min);
        DragFloat("EV Max", &settings.agx.ev_max);
        SliderFloat("Mid Gray", &settings.agx.mid_gray, 0.0, 5.0);
        SliderFloat("Offset", &settings.agx.offset, -1.0, 1.0);
        SliderFloat("Slope", &settings.agx.slope, 0.0, 5.0);
        SliderFloat("Power", &settings.agx.power, 0.0, 5.0);
        SliderFloat("Saturation", &settings.agx.saturation, 0.0, 5.0);
        PopID();
    }
    if (CollapsingHeader("Sky")) {
        PushID("sky");
        SliderFloat("EV", &settings.sky.exposure, -8.0f, 8.0f);
        PopID();
    }

    End();
}
