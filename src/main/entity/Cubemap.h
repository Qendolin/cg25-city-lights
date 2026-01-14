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
    static constexpr int FACES_COUNT = 6;
    static constexpr vk::Format FORMAT = vk::Format::eE5B9G9R9UfloatPack32;

    vma::Allocator allocator;
    vk::Device device;

    Image image;
    ImageView view;

public:
    Cubemap(const vma::Allocator &allocator,
            const vk::Device &device,
            const DeviceQueue &transferQueue,
            const DeviceQueue &graphicsQueue,
            const std::array<std::string, 6> &skyboxImageFilenames);

    const ImageViewBase& getImageView() const { return view; }

    static std::array<std::string, 6> makeSkyboxImageFilenames(const std::filesystem::path& directory);

private:
    static std::vector<uint32_t> getPixelData(const std::array<PlainImageDataU32, FACES_COUNT> &plainImages);

    // Packs three positive floats (R,G,B) into VK_FORMAT_E5B9G9R9_UFLOAT_PACK32
    static uint32_t packRGB9E5(float r, float g, float b) {
        // Clamp negatives to zero – format cannot represent them
        r = std::max(r, 0.0f);
        g = std::max(g, 0.0f);
        b = std::max(b, 0.0f);

        // Find max channel
        float maxRGB = std::max(r, std::max(g, b));

        if (maxRGB < 1e-20f) {
            // All zero
            return 0;
        }

        // Compute exponent: e = floor(log2(max)) + 1 + bias
        // Bias = 15 (5-bit exponent, unbiased range -15..16)
        int exponent = std::max(-15, (int) std::floor(std::log2(maxRGB)) + 1);

        // Compute shared exponent scaling factor
        float scale = std::ldexp(1.0f, 9 - exponent); // 2^(9 - e)

        // Quantize mantissas
        uint32_t R = (uint32_t) std::min(511.0f, std::floor(r * scale + 0.5f));
        uint32_t G = (uint32_t) std::min(511.0f, std::floor(g * scale + 0.5f));
        uint32_t B = (uint32_t) std::min(511.0f, std::floor(b * scale + 0.5f));

        // Re-bias exponent
        uint32_t E = (uint32_t) (exponent + 15);

        // Pack bits: RRRRRRRRR GGGGGGGGG BBBBBBBBB EEEEE
        return (E << 27) | (B << 18) | (G << 9) | R;
    }

    // Expects R32G32B32_SFloat
    static void convertImageToRGB9E5(const float *src, uint32_t *dst, size_t width, size_t height) {
        size_t pixels = width * height;

        for (size_t i = 0; i < pixels; i++) {
            float r = src[i * 3 + 0];
            float g = src[i * 3 + 1];
            float b = src[i * 3 + 2];
            dst[i] = packRGB9E5(r, g, b);
        }
    }
};
