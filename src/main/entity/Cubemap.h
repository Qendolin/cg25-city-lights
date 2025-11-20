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
    static constexpr vk::Format FORMAT{vk::Format::eR8G8B8A8Srgb};

    vma::Allocator allocator;
    vk::Device device;

    Image image;
    vk::UniqueImageView view;

public:
    Cubemap(const vma::Allocator &allocator,
            const vk::Device &device,
            const DeviceQueue &transferQueue,
            const DeviceQueue &graphicsQueue,
            const std::array<std::string, 6> &skyboxImageFilenames);

    vk::ImageView getImageView() const { return *view; }
    
private:
    static std::vector<unsigned char> getPixelData(std::array<PlainImageData, FACES_COUNT> plainImages);
};
