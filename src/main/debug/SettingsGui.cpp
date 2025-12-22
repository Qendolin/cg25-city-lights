#include "SettingsGui.h"

#include <format>
#include <glm/gtc/type_ptr.hpp>
#include <string>

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

    if (CollapsingHeader("Shadows")) {
        Checkbox("Update", &settings.shadowCascade.update);
        Checkbox("Visualize", &settings.shadowCascade.visualize);
        SliderFloat("Split Lambda", &settings.shadowCascade.lambda, 0.0f, 1.0f);
        DragFloat("Distance", &settings.shadowCascade.distance);
        Indent();
        for (size_t i = 0; i < settings.shadowCascades.size(); i++) {
            if (CollapsingHeader(std::format("Shadow Cascade {}", i).c_str())) {
                PushID(std::format("shadow_{}", i).c_str());
                auto &cascade = settings.shadowCascades[i];
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
        Unindent();
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
        ColorEdit3("Tint", glm::value_ptr(settings.sky.tint), ImGuiColorEditFlags_Float);
        PopID();
    }

    if (CollapsingHeader("Rendering")) {
        PushID("rendering");
        ColorEdit3("Ambient", glm::value_ptr(settings.rendering.ambient), ImGuiColorEditFlags_Float);
        Checkbox("Frustum Culling", &settings.rendering.enableFrustumCulling);
        Checkbox("Async Compute", &settings.rendering.asyncCompute);
        Checkbox("Pause Culling", &settings.rendering.pauseFrustumCulling);
        Checkbox("White World", &settings.rendering.whiteWorld);
        Checkbox("Light Density", &settings.rendering.lightDensity);
        SliderFloat("Light Range Factor", &settings.rendering.lightRangeFactor, 0.0f, 1.0f);
        Text("Settings below require a resource reload.");
        if (BeginCombo("MSAA", std::format("x{}", settings.rendering.msaa).c_str())) {
            for (int msaa : {1,2,4,8}) {
                bool is_selected = settings.rendering.msaa == msaa; // You can store your selection however you want, outside or inside your objects
                if (Selectable(std::format("x{}", msaa).c_str(), is_selected))
                    settings.rendering.msaa = msaa;
            }
            EndCombo();
        }
        PopID();
    }

    if (CollapsingHeader("SSAO")) {
        PushID("ssao");
        Checkbox("Update", &settings.ssao.update);
        SliderFloat("Radius", &settings.ssao.radius, 0.0f, 4.0f);
        SliderFloat("Exponent", &settings.ssao.exponent, 0.0f, 4.0f);
        DragFloat("Filter Sharpness", &settings.ssao.filterSharpness, 1, 0, 200);
        SliderFloat("Depth Bias", &settings.ssao.bias, 0.0f, 0.1f);
        Text("Settings below require a resource reload.");
        Checkbox("Half Resolution", &settings.ssao.halfResolution);
        Checkbox("Bent Normals", &settings.ssao.bentNormals);
        SliderInt("Slices", &settings.ssao.slices, 1, 16);
        SliderInt("Samples", &settings.ssao.samples, 1, 32);

        PopID();
    }

    if (CollapsingHeader("Animation")) {
        PushID("animation");
        Checkbox("Render Blob", &settings.animation.renderBlob);
        Checkbox("Animate Blob Node", &settings.animation.animateBlobNode);
        SliderFloat("Playback Speed", &settings.animation.playbackSpeed, 0.0f, 4.0f);
        SliderFloat("Timeline", &settings.animation.time, 0.0f, 60.0f);
        Checkbox("Pause Animation", &settings.animation.pause);

        PopID();
    }

    if (CollapsingHeader("Camera")) {
        PushID("camera");
        Checkbox("Debug Camera", &settings.camera.debugCamera);
        PopID();
    }

    End();
}
