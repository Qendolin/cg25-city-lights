#pragma once

#include <array>
#include <string>
#include <vulkan/vulkan.hpp>

#include "../backend/DeviceQueue.h"
#include "../backend/Image.h"

namespace vma {
    class Allocator;
}

struct DeviceQueue;

class Cubemap {
private:
    static constexpr int FACES_COUNT{6};

    vma::Allocator allocator;
    vk::Device device;

    vk::Image image;
    vma::Allocation imageAlloc;
    vk::UniqueImageView view;

public:
    Cubemap(const vma::Allocator &allocator,
           const vk::Device &device,
           const DeviceQueue &queue,
           const std::array<std::string, 6> &skyboxImageFilenames);
    ~Cubemap();

private:
    void transitionImageLayout(const vk::CommandBuffer &commandBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const;
};
