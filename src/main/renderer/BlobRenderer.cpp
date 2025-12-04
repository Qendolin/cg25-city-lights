#include "BlobRenderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../blob/VertexData.h"
#include "../debug/Annotation.h"
#include "../util/globals.h"
#include "../util/math.h"

BlobRenderer::BlobRenderer(const vk::Device &device)
    : mComputeDescriptorLayout{device} {
}

void BlobRenderer::recreate(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
    createComputePipeline_(device, shaderLoader);
    createGraphicsPipeline_(device, shaderLoader, framebuffer);
}

void BlobRenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &commandBuffer,
        const Framebuffer &framebuffer,
        const Camera &camera,
        const blob::Model &blobModel
) {
    computeVertices(device, allocator, commandBuffer, blobModel);
    renderVertices(commandBuffer, framebuffer, camera, blobModel);
}

void BlobRenderer::createComputePipeline_(const vk::Device &device, const ShaderLoader &shaderLoader) {
    UniqueCompiledShaderStage compShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.comp");

    vk::PushConstantRange pushConstantRange{vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstant)};

    ComputePipelineConfig pipelineConfig{};
    pipelineConfig.descriptorSetLayouts = {mComputeDescriptorLayout};
    pipelineConfig.pushConstants = {pushConstantRange};

    mComputePipeline = createComputePipeline(device, pipelineConfig, *compShader);
}

void BlobRenderer::createGraphicsPipeline_(
        const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer
) {
    UniqueCompiledShaderStage vertShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.vert");
    UniqueCompiledShaderStage fragShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.frag");

    vk::PushConstantRange pushConstantRange{
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(VertexFragmentPushConstant)
    };

    GraphicsPipelineConfig pipelineConfig{};
    pipelineConfig.vertexInput = {blob::VertexData::getBindingDescriptions(), blob::VertexData::getAttributeDescriptions()};
    pipelineConfig.descriptorSetLayouts = {};
    pipelineConfig.pushConstants = {pushConstantRange};
    pipelineConfig.attachments = {framebuffer.colorFormats(), framebuffer.depthFormat()};

    mGraphicsPipeline = createGraphicsPipeline(device, pipelineConfig, {*vertShader, *fragShader});
}

void BlobRenderer::computeVertices(const vk::Device &device, const DescriptorAllocator &allocator, const vk::CommandBuffer &commandBuffer, const blob::Model &blobModel) {    
    util::ScopedCommandLabel dbg_cmd_label_func(commandBuffer);

    vk::DrawIndirectCommand drawIndirectCommand{};
    drawIndirectCommand.vertexCount = 0;
    drawIndirectCommand.instanceCount = 1;
    drawIndirectCommand.firstVertex = 0;
    drawIndirectCommand.firstInstance = 0;

    vk::Buffer indirectDrawBuffer = blobModel.getIndirectDrawBuffer();

    commandBuffer.updateBuffer(indirectDrawBuffer, 0, sizeof(vk::DrawIndirectCommand), &drawIndirectCommand);

    vk::BufferMemoryBarrier barrier{};
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.buffer = indirectDrawBuffer;
    barrier.offset = 0;
    barrier.size = vk::WholeSize;

    commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, 1,
            &barrier, 0, nullptr
    );

    DescriptorSet set = allocator.allocate(mComputeDescriptorLayout);

    vk::DescriptorBufferInfo vertexBufferInfo{
        .buffer = blobModel.getVertexBuffer(),
        .offset = 0,
        .range = vk::WholeSize,
    };

    vk::DescriptorBufferInfo indirectDrawBufferInfo{
        .buffer = blobModel.getIndirectDrawBuffer(),
        .offset = 0,
        .range = vk::WholeSize,
    };

    std::array<vk::WriteDescriptorSet, 2> writes{
        set.write(ComputeDescriptorLayout::VERTICES_BINDING, vertexBufferInfo),
        set.write(ComputeDescriptorLayout::INDIRECT_DRAW_BINDING, indirectDrawBufferInfo),
    };

    device.updateDescriptorSets(writes, {});
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *mComputePipeline.pipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mComputePipeline.layout, 0, {set}, {});

    const int resolution = blobModel.getResolution();

    ComputePushConstant pc{};
    pc.resolution = resolution;
    pc.time = blobModel.getTime();

    commandBuffer.pushConstants(
            *mComputePipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstant), &pc
    );

    uint32_t groups = (resolution + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE;

    commandBuffer.dispatch(groups, groups, groups);

    vk::BufferMemoryBarrier barriers[2]{};

    barriers[0].srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    barriers[0].dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead;
    barriers[0].srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barriers[0].dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barriers[0].buffer = blobModel.getVertexBuffer();
    barriers[0].offset = 0;
    barriers[0].size = vk::WholeSize;

    barriers[1].srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    barriers[1].dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead;
    barriers[1].srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barriers[1].dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barriers[1].buffer = blobModel.getIndirectDrawBuffer();
    barriers[1].offset = 0;
    barriers[1].size = vk::WholeSize;

    commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eVertexInput | vk::PipelineStageFlagBits::eDrawIndirect, {}, 0, nullptr, 2,
            barriers, 0, nullptr
    );
}

void BlobRenderer::renderVertices(
        const vk::CommandBuffer &commandBuffer, const Framebuffer &framebuffer, const Camera &camera, const blob::Model &blobModel
) {
    util::ScopedCommandLabel dbg_cmd_label_func(commandBuffer);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *mGraphicsPipeline.pipeline);

    mGraphicsPipeline.config.viewports = {{framebuffer.viewport(true)}};
    mGraphicsPipeline.config.scissors = {{framebuffer.area()}};
    mGraphicsPipeline.config.apply(commandBuffer);

    VertexFragmentPushConstant push{};
    glm::mat4 ModelMatrix = blobModel.getModelMatrix();
    push.projectionViewModel = camera.projectionMatrix() * camera.viewMatrix() * ModelMatrix;
    push.ModelMatrix = ModelMatrix;

    commandBuffer.pushConstants(
            *mGraphicsPipeline.layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(VertexFragmentPushConstant), &push
    );

    commandBuffer.beginRendering(framebuffer.renderingInfo({
        .enableColorAttachments = true,
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eLoad},
        .colorStoreOps = {vk::AttachmentStoreOp::eStore},
        .depthLoadOp = vk::AttachmentLoadOp::eLoad,
    }));

    commandBuffer.bindVertexBuffers(0, {blobModel.getVertexBuffer()}, {0});
    commandBuffer.drawIndirect(blobModel.getIndirectDrawBuffer(), 0, 1, sizeof(vk::DrawIndirectCommand));
    commandBuffer.endRendering();
}
