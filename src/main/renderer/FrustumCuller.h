#pragma once

#include <array>
#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../util/PerFrame.h"


namespace scene {
    struct GpuData;
}
class Buffer;
class ShaderLoader;

class FrustumCuller {

public:
    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding InputDrawCommandBuffer{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding OutputDrawCommandBuffer{1, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding DrawCommandCountBuffer{2, vk::ShaderStageFlagBits::eCompute};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, InputDrawCommandBuffer, OutputDrawCommandBuffer, DrawCommandCountBuffer);
        }
    };

    struct alignas(16) ShaderParamsPushConstants {
        std::array<glm::vec4, 6> planes;
    };

    ~FrustumCuller();
    FrustumCuller(const vk::Device &device, const DescriptorAllocator &allocator);


    void recreate(const vk::Device &device, const ShaderLoader &shader_loader) {
        createPipeline(device, shader_loader);
    }

    void execute(
            const vk::Device &device,
            const vk::CommandBuffer &cmd_buf,
            const scene::GpuData &gpu_data,
            const glm::mat4 &view_projection_matrix,
            Buffer &output_draw_command_buffer,
            Buffer &output_draw_command_count_buffer,
            size_t count_write_index
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);

    ConfiguredComputePipeline mPipeline;
    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
    util::PerFrame<DescriptorSet> mShaderParamsDescriptors;
};
