#include "BlobRenderer.h"

#include "../blob/System.h"
#include "../blob/VertexData.h"
#include "../debug/Annotation.h"
#include "../entity/Light.h"
#include "../util/math.h"

BlobRenderer::BlobRenderer(const vk::Device &device) : mComputeDescriptorLayout{device}, mDrawDescriptorLayout(device) {

    mSampler = device.createSamplerUnique({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
    });

    // I have no idea why, but the blob rendering just doesn't work with the shared descriptor allocator.
    // I'm 99% sure it's a driver issue.
    mDescriptorPools.create(globals::MaxFramesInFlight, [&device] {
        std::vector poolSizes = {
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 16),
            vk::DescriptorPoolSize(vk::DescriptorType::eInlineUniformBlock, 1024),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 16 * 4),
        };
        vk::DescriptorPoolInlineUniformBlockCreateInfo inlineUniformInfo{};
        inlineUniformInfo.maxInlineUniformBlockBindings = 1024;
        return device.createDescriptorPoolUnique({
            .pNext = &inlineUniformInfo,
            .maxSets = 16,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data(),
        });
    });
}

void BlobRenderer::recreate(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
    createPipelines(device, shaderLoader, framebuffer);
}

void BlobRenderer::compute(
        const vk::Device &device, const vk::CommandBuffer &cmd_buf, const blob::System &blobSystem, float timestamp
) {
    util::ScopedCommandLabel dbg_cmd_label_func(cmd_buf, "Compute");

    const BufferBase &indirect_buffer = blobSystem.drawIndirectBuffer();
    const BufferBase &vertex_buffer = blobSystem.vertexBuffer();
    const BufferBase &metaball_buffer = blobSystem.metaballBuffer();
    const BufferBase &domain_member_buffer = blobSystem.domainMemberBuffer();

    auto domains = blobSystem.domains();

    // We must zero the vertexCount and set instanceCount/firstVertex before dispatch.
    std::vector<vk::DrawIndirectCommand> drawCommands(domains.size());
    size_t cumulative_vertex_offset = 0;

    for (size_t i = 0; i < domains.size(); ++i) {
        drawCommands[i].vertexCount = 0;
        drawCommands[i].instanceCount = 1;
        drawCommands[i].firstVertex = cumulative_vertex_offset;
        drawCommands[i].firstInstance = 0;

        cumulative_vertex_offset += blobSystem.estimateVertexCount(domains[i]);
    }

    indirect_buffer.barrier(cmd_buf, BufferResourceAccess::TransferWrite);
    cmd_buf.updateBuffer(indirect_buffer, 0, drawCommands.size() * sizeof(vk::DrawIndirectCommand), drawCommands.data());

    indirect_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderStorageReadWrite);
    vertex_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderStorageWrite);
    metaball_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderStorageRead);
    domain_member_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderStorageRead);

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mComputePipeline.pipeline);

    mDescriptorPools.next();
    device.resetDescriptorPool(*mDescriptorPools.get());

    auto desc_layout = static_cast<vk::DescriptorSetLayout>(mComputeDescriptorLayout);
    DescriptorSet set(device.allocateDescriptorSets({
        .descriptorPool = *mDescriptorPools.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &desc_layout,
    })[0]);

    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mComputePipeline.layout, 0, {set}, {});
    device.updateDescriptorSets(
            {
                set.write(ComputeDescriptorLayout::MetaballBuffer, {.buffer = metaball_buffer, .range = vk::WholeSize}),
                set.write(ComputeDescriptorLayout::VertexBuffer, {.buffer = vertex_buffer, .range = vk::WholeSize}),
                set.write(ComputeDescriptorLayout::IndirectBuffer, {.buffer = indirect_buffer, .range = vk::WholeSize}),
                set.write(ComputeDescriptorLayout::DomainMemberBuffer, {.buffer = domain_member_buffer, .range = vk::WholeSize}),
            },
            {}
    );

    size_t metaball_index_offset = 0;

    for (size_t i = 0; i < domains.size(); i++) {
        const blob::Domain &domain = domains[i];
        ComputePushConstant push = {
            .aabbMin = domain.bounds.min,
            .cellSize = blobSystem.cellSize,
            .aabbMax = domain.bounds.max,
            .time = timestamp,
            .globalGridOrigin = blobSystem.origin,
            .metaballIndexOffset = static_cast<glm::uint>(metaball_index_offset),
            .metaballCount = static_cast<glm::uint>(domain.members.size()),
            .groundLevel = blobSystem.groundLevel,
            .drawIndex = static_cast<glm::uint>(i),
            .firstVertex = drawCommands[i].firstVertex,
        };

        cmd_buf.pushConstants(
                *mComputePipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstant), &push
        );

        glm::vec3 domain_size = domain.bounds.max - domain.bounds.min;
        glm::ivec3 cell_count;
        cell_count.x = std::ceil(domain_size.x / blobSystem.cellSize);
        cell_count.y = std::ceil(domain_size.y / blobSystem.cellSize);
        cell_count.z = std::ceil(domain_size.z / blobSystem.cellSize);

        cmd_buf.dispatch(util::divCeil(cell_count.x, 4), util::divCeil(cell_count.y, 4), util::divCeil(cell_count.z, 4));

        metaball_index_offset += domain.members.size();
    }
}

void BlobRenderer::draw(
        const vk::Device &device,
        const vk::CommandBuffer &cmd_buf,
        const Framebuffer &framebuffer,
        const ImageViewPairBase &storedColorImage,
        const Camera &camera,
        const DirectionalLight &sun,
        const glm::vec3 &ambientLight,
        const blob::System &blobSystem
) {
    util::ScopedCommandLabel dbg_cmd_label_func(cmd_buf, "Draw");

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *mGraphicsPipeline.pipeline);

    mGraphicsPipeline.config.viewports = {{framebuffer.viewport(true)}};
    mGraphicsPipeline.config.scissors = {{framebuffer.area()}};
    mGraphicsPipeline.config.apply(cmd_buf);

    auto desc_layout = static_cast<vk::DescriptorSetLayout>(mDrawDescriptorLayout);
    DescriptorSet set(device.allocateDescriptorSets({
        .descriptorPool = *mDescriptorPools.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = &desc_layout,
    })[0]);

    framebuffer.colorAttachments[0].image().barrier(cmd_buf, ImageResourceAccess::ColorAttachmentWrite);
    storedColorImage.image().barrier(cmd_buf, ImageResourceAccess::FragmentShaderReadOptimal);

    DrawInlineUniformBlock params = {
        .projectionViewMatrix = camera.projectionMatrix() *  camera.viewMatrix(),
        .modelMatrix = glm::translate(glm::mat4(1.0f), blobSystem.origin),
        .camera = glm::vec4(camera.position, 0.0),
        .invViewportSize = 1.0f / glm::vec2(framebuffer.area().extent.width, framebuffer.area().extent.height),
        .sunDir = glm::vec4(sun.direction(), 0.0),
        .sunLight = glm::vec4(sun.radiance(), 0.0),
        .ambientLight = glm::vec4(ambientLight, 0.0),
    };

    device.updateDescriptorSets(
            {set.write(
                     DrawDescriptorLayout::StoredColorImage,
                     {
                         .sampler = *mSampler,
                         .imageView = storedColorImage,
                         .imageLayout = ImageResourceAccess::FragmentShaderReadOptimal.layout,
                     }
             ),
             set.write(
                     DrawDescriptorLayout::ShaderParams,
                     {
                         .dataSize = sizeof(params),
                         .pData = &params,
                     }
             )},
            {}
    );

    const BufferBase &indirect_buffer = blobSystem.drawIndirectBuffer();
    const BufferBase &vertex_buffer = blobSystem.vertexBuffer();

    indirect_buffer.barrier(cmd_buf, BufferResourceAccess::IndirectCommandRead);
    vertex_buffer.barrier(cmd_buf, BufferResourceAccess::VertexShaderAttributeRead);

    cmd_buf.beginRendering(framebuffer.renderingInfo({
        .enableColorAttachments = true,
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eLoad},
        .colorStoreOps = {vk::AttachmentStoreOp::eStore},
        .depthLoadOp = vk::AttachmentLoadOp::eLoad,
    }));

    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mGraphicsPipeline.layout, 0, {set}, {});
    cmd_buf.bindVertexBuffers(0, {vertex_buffer}, {0});
    cmd_buf.drawIndirect(indirect_buffer, 0, blobSystem.domains().size(), sizeof(vk::DrawIndirectCommand));
    cmd_buf.endRendering();
}

void BlobRenderer::createPipelines(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
    {
        UniqueCompiledShaderStage compShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.comp");

        vk::PushConstantRange pushConstantRange{vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstant)};

        ComputePipelineConfig pipelineConfig{};
        pipelineConfig.descriptorSetLayouts = {mComputeDescriptorLayout};
        pipelineConfig.pushConstants = {pushConstantRange};

        mComputePipeline = createComputePipeline(device, pipelineConfig, *compShader);
        util::setDebugName(device, *mComputePipeline.pipeline, "blob_compute");
    }

    {
        UniqueCompiledShaderStage vertShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.vert");
        UniqueCompiledShaderStage fragShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.frag");


        GraphicsPipelineConfig pipelineConfig{};
        pipelineConfig.vertexInput = {
            blob::VertexData::getBindingDescriptions(), blob::VertexData::getAttributeDescriptions()
        };
        pipelineConfig.descriptorSetLayouts = {mDrawDescriptorLayout};
        pipelineConfig.pushConstants = {};
        pipelineConfig.attachments = {framebuffer.colorFormats(), framebuffer.depthFormat()};
        pipelineConfig.rasterizer.samples = framebuffer.depthAttachment.image().info.samples;
        pipelineConfig.cull.mode = vk::CullModeFlagBits::eNone;

        mGraphicsPipeline = createGraphicsPipeline(device, pipelineConfig, {*vertShader, *fragShader});
        util::setDebugName(device, *mGraphicsPipeline.pipeline, "blob_draw");
    }
}
