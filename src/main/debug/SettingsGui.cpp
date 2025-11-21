#include "SettingsGui.h"

#include <format>
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
    for (size_t i = 0; i < settings.shadowCascades.size(); i++) {
        if (CollapsingHeader(std::format("Shadow Cascade {}", i).c_str())) {
            PushID(std::format("shadow_{}", i).c_str());
            auto& cascade = settings.shadowCascades[i];
            DragFloat("Dimension", &cascade.dimension);
            DragFloat("Start", &cascade.start);
            DragFloat("End", &cascade.end);
            SliderFloat("Extrusion Bias", &cascade.extrusionBias, -10, 10);
            DragFloat("Normal Bias", &cascade.normalBias);
            SliderFloat("Sample Bias", &cascade.sampleBias, 0.0f, 10.0f);
            SliderFloat("Sample Bias Clamp", &cascade.sampleBiasClamp, 0.0f, 1.0f, "%.5f");
            DragFloat("Depth Bias Const", &cascade.depthBiasConstant);
            SliderFloat("Depth Bias Slope", &cascade.depthBiasSlope, -2.5f, 2.5f, "%.5f");
            SliderFloat("Depth Bias Clamp", &cascade.depthBiasClamp, 0.0f, 0.1f, "%.5f");
            PopID();
        }
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


    if (CollapsingHeader("Rendering")) {
        PushID("rendering");
        ColorEdit3("Ambient", glm::value_ptr(settings.rendering.ambient), ImGuiColorEditFlags_Float);
        Checkbox("Frustum Culling", &settings.rendering.enableFrustumCulling);
        Checkbox("Pause", &settings.rendering.pauseFrustumCulling);
        PopID();
    }

    End();
}
