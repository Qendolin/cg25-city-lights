#pragma once

#include <array>
#include <limits>

struct FrameTimes {
    int singleIndex = 0;
    int cumulativeIndex = 0;
    // current frame time in seconds
    float current = 0;
    float currentMin = std::numeric_limits<float>::infinity();
    float currentMax = -std::numeric_limits<float>::infinity();
    float currentAvg = 0;

    float nextMin = 0;
    float nextMax = 0;
    int nextAvgSum = 0;
    float nextAvgTimer = 0;
    // history frame times in ms
    std::array<float, 128> single = {};
    std::array<float, 32> avg = {};
    std::array<float, 32> min = {};
    std::array<float, 32> max = {};

    void update(float delta);
    void draw();
};
