#pragma once
#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../debug/Annotation.h"
#include "../debug/Settings.h"


struct ImageViewPairBase;
class ShaderLoader;
namespace vk {
    class Device;
}

class FinalizeRenderer {

public:
    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr CombinedImageSamplerBinding InColor{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageImageBinding OutColor{1, vk::ShaderStageFlagBits::eCompute};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, InColor, OutColor);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "finalize_renderer_descriptor_layout");
        }
    };

    ~FinalizeRenderer();
    explicit FinalizeRenderer(const vk::Device &device);


    void recreate(const vk::Device &device, const ShaderLoader &shader_loader) {
        createPipeline(device, shader_loader);
    }

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &descriptor_allocator,
            const vk::CommandBuffer &cmd_buf,
            const ImageViewPairBase &hdr_attachment,
            const ImageViewPairBase &sdr_attachment,
            const Settings::AgXParams &agx_params
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);

    vk::UniqueSampler mSampler;
    ConfiguredComputePipeline mPipeline;
    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
};
