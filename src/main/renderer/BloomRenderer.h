#pragma once
#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../debug/Annotation.h"
#include "../debug/Settings.h"


struct ImageResourceAccess;
class ImageViewPair;
class TransientImageViewPair;
struct ImageViewBase;
struct ImageView;
struct Image;
namespace vma {
    class Allocator;
}
struct ImageViewPairBase;
class ShaderLoader;
class BloomRenderer {

public:
    static constexpr int LEVELS = 5;

    struct UpDescriptorLayout : DescriptorSetLayout {
        static constexpr SampledImageBinding InCurrColor{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr CombinedImageSamplerBinding InPrevColor{1, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageImageBinding OutColor{2, vk::ShaderStageFlagBits::eCompute};

        UpDescriptorLayout() = default;

        explicit UpDescriptorLayout(const vk::Device &device) {
            create(device, {}, InCurrColor, InPrevColor, OutColor);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "bloom_up_descriptor_layout");
        }
    };

    struct DownDescriptorLayout : DescriptorSetLayout {
        static constexpr CombinedImageSamplerBinding InColor{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageImageBinding OutColor{1, vk::ShaderStageFlagBits::eCompute};

        DownDescriptorLayout() = default;

        explicit DownDescriptorLayout(const vk::Device &device) {
            create(device, {}, InColor, OutColor);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "bloom_down_descriptor_layout");
        }
    };

    struct UpPushConstants {
        float prevFactor;
        float currFactor;
        int lastPass;
    };

    struct DownPushConstants {
        glm::vec3 thresholdCurve;
        float threshold;
        int firstPass;
    };

    float threshold = 1.0;
    float knee = 0.6f;
    std::array<float, LEVELS> factors = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    explicit BloomRenderer(const vk::Device &device);

    void recreate(
            const vk::Device &device, const vma::Allocator &allocator, const ShaderLoader &shader_loader, vk::Extent2D viewport_extent
    ) {
        createPipeline(device, shader_loader);
        createImages(device, allocator, viewport_extent);
    }

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const ImageViewPairBase &hdr_attachment
    );

    const ImageViewBase &result() const;

private:
    void downPass(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const ImageViewPairBase &in_image,
            const ImageViewPairBase &out_image,
            int level
    );

    void upPass(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            float prev_factor,
            const ImageViewPairBase &in_prev_image,
            float curr_factor,
            const ImageViewPairBase &in_curr_image,
            const ImageViewPairBase &out_image,
            int level
    );

    static void barrier(
            const ImageViewPairBase &image,
            std::span<ImageResourceAccess> tracker,
            const vk::CommandBuffer &cmd_buf,
            const ImageResourceAccess &current
    );

    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);
    void createImages(const vk::Device &device, const vma::Allocator &allocator, vk::Extent2D viewport_extent);

    ConfiguredComputePipeline mUpPipeline;
    UpDescriptorLayout mUpDescriptorLayout;
    vk::UniqueSampler mUpSampler;
    std::unique_ptr<Image> mUpImage;
    std::vector<ImageView> mUpImageViews;
    std::vector<ImageResourceAccess> mUpImageAccess;

    ConfiguredComputePipeline mDownPipeline;
    DownDescriptorLayout mDownDescriptorLayout;
    vk::UniqueSampler mDownSampler;
    std::unique_ptr<Image> mDownImage;
    std::vector<ImageView> mDownImageViews;
    std::vector<ImageResourceAccess> mDownImageAccess;
};
