#include "Performance.h"

#include <cmath>
#include <format>
#include <imgui.h>
#include <string>

void FrameTimes::draw() {
    using namespace ImGui;

    SetNextWindowPos(ImVec2(1330, 0), ImGuiCond_FirstUseEver);
    SetNextWindowSize(ImVec2(270, 450), ImGuiCond_FirstUseEver);
    Begin("Performance", nullptr, 0);

    constexpr auto sixty_fps_line_color = 0x808000ff;

    auto draw_list = GetWindowDrawList();
    Text("%4d fps", current <= 0.00001f ? 0 : static_cast<int>(1.0f / current));
    single[singleIndex] = current * 1000;
    singleIndex = (singleIndex + 1) % static_cast<int>(single.size());
    auto sixty_fps_line_point = GetCursorScreenPos() + ImVec2{0, 48};
    std::string frame_time_text = std::format("Frame Time - {:4.1f} ms", current * 1000);
    PlotHistogram(
            "##frame_time", &single.front(), single.size(), singleIndex, frame_time_text.c_str(), 0.0f, 1000.0f / 30.0f,
            ImVec2{256, 96}
    );
    draw_list->AddLine(sixty_fps_line_point, sixty_fps_line_point + ImVec2{256, 0}, sixty_fps_line_color);

    sixty_fps_line_point = GetCursorScreenPos() + ImVec2{0, 48};
    std::string frame_avg_text = std::format("Avg. Frame Time - {:4.1f} ms", currentAvg * 1000);
    PlotLines(
            "##frame_time_avg", &avg.front(), avg.size(), cumulativeIndex, frame_avg_text.c_str(), 0, 1000.0f / 30.0f,
            ImVec2{256, 96}
    );
    draw_list->AddLine(sixty_fps_line_point, sixty_fps_line_point + ImVec2{256, 0}, sixty_fps_line_color);

    sixty_fps_line_point = GetCursorScreenPos() + ImVec2{0, 48};
    std::string frame_min_text = std::format("Min. Frame Time - {:4.1f} ms", currentMin * 1000);
    PlotLines(
            "##frame_time_min", &min.front(), min.size(), cumulativeIndex, frame_min_text.c_str(), 0, 1000.0f / 30.0f,
            ImVec2{256, 96}
    );
    draw_list->AddLine(sixty_fps_line_point, sixty_fps_line_point + ImVec2{256, 0}, sixty_fps_line_color);

    sixty_fps_line_point = GetCursorScreenPos() + ImVec2{0, 48};
    std::string frame_max_text = std::format("Max. Frame Time - {:4.1f} ms", currentMax * 1000);
    PlotLines(
            "##frame_time_max", &max.front(), max.size(), cumulativeIndex, frame_max_text.c_str(), 0, 1000.0f / 30.0f,
            ImVec2{256, 96}
    );
    draw_list->AddLine(sixty_fps_line_point, sixty_fps_line_point + ImVec2{256, 0}, sixty_fps_line_color);

    for (const auto & line : lines) {
        Text("%s: %.3f", line.first.c_str(), line.second);
    }
    lines.clear();

    End();
}


void FrameTimes::update(float delta) {
    current = delta;
    if (current < nextMin)
        nextMin = current;
    if (current > nextMax)
        nextMax = current;

    nextAvgSum += 1;
    nextAvgTimer += current;

    if (nextAvgTimer >= 1.0) {
        currentAvg = nextAvgTimer / static_cast<float>(nextAvgSum);
        currentMin = std::isinf(nextMin) ? 0 : nextMin;
        currentMax = std::isinf(nextMax) ? 0 : nextMax;

        avg[cumulativeIndex] = currentAvg * 1000;
        min[cumulativeIndex] = currentMin * 1000;
        max[cumulativeIndex] = currentMax * 1000;

        cumulativeIndex = (cumulativeIndex + 1) % static_cast<int>(avg.size());
        nextMin = std::numeric_limits<float>::infinity();
        nextMax = -std::numeric_limits<float>::infinity();
        nextAvgTimer = 0;
        nextAvgSum = 0;
    }
}
