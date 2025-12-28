#pragma once

#include <array>
#include <glm/glm.hpp>

#include "../backend/Buffer.h"
#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../debug/Annotation.h"

class TransientBufferAllocator;
namespace scene {
    struct GpuData;
}
struct Buffer;
class ShaderLoader;

class FrustumCuller {

public:

    struct alignas(16) ShaderParamsInlineUniformBlock {
        std::array<glm::vec4, 6> planes = {};
        std::array<glm::vec4, 6> excludePlanes = {};
        float minWorldRadius = 0.0f;
        glm::uint enableExcludePlanes = 0;
        float pad0 = 0;
        float pad1 = 0;
    };

    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding InputDrawCommandBuffer{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding OutputDrawCommandBuffer{1, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding DrawCommandCountBuffer{2, vk::ShaderStageFlagBits::eCompute};
        static constexpr InlineUniformBlockBinding ShaderParams{3, vk::ShaderStageFlagBits::eCompute, sizeof(ShaderParamsInlineUniformBlock)};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, InputDrawCommandBuffer, OutputDrawCommandBuffer, DrawCommandCountBuffer, ShaderParams);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "frustum_culler_descriptor_layout");
        }
    };

    ~FrustumCuller();
    explicit FrustumCuller(const vk::Device &device);


    void recreate(const vk::Device &device, const ShaderLoader &shader_loader) {
        createPipeline(device, shader_loader);
    }

    /// <summary>
    /// Executes the frustum culling compute shader.
    /// The shader takes a list of all draw commands and outputs a filtered list of draw commands
    /// that are inside the frustum.
    /// The count of the output draw commands is stored at the end of the output buffer, at offset `buffer_size - 32`.
    /// </summary>
    /// <param name="device">The Vulkan device.</param>
    /// <param name="desc_alloc">The descriptor allocator.</param>
    /// <param name="buf_alloc">The transient buffer allocator.</param>
    /// <param name="cmd_buf">The command buffer to record commands to.</param>
    /// <param name="gpu_data">The scene's GPU data, containing the input draw commands.</param>
    /// <param name="view_projection_matrix">The view-projection matrix of the camera, used to extract frustum planes.</param>
    /// <param name="exclude_frustum">An optional frustum to exclude objects from. Objects inside this frustum will be culled.</param>
    /// <param name="min_world_radius">Minimum world radius of objects to be culled. Objects smaller than this will always be culled.</param>
    /// <returns>A buffer containing the culled draw commands. The draw command count is at offset `buffer_size - 32`.</returns>
    UnmanagedBuffer execute(
            const vk::Device &device,
            const DescriptorAllocator &desc_alloc,
            const TransientBufferAllocator &buf_alloc,
            const vk::CommandBuffer &cmd_buf,
            const scene::GpuData &gpu_data,
            const glm::mat4 &view_projection_matrix,
            const glm::mat4 *exclude_frustum = nullptr,
            float min_world_radius = 0.0
    ) const;

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);

    ConfiguredComputePipeline mPipeline;
    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
};
