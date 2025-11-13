#include "BlobRenderer2.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../blob/model/VertexData.h"
#include "../util/globals.h"
#include "../util/math.h"

BlobRenderer2::BlobRenderer2(const vk::Device &device, const DescriptorAllocator &allocator)
    : mComputeDescriptorLayout{device} {
    mComputeDescriptors.create(util::MaxFramesInFlight, [&]() { return allocator.allocate(mComputeDescriptorLayout); });
}

void BlobRenderer2::recreate(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
    createComputePipeline_(device, shaderLoader);
    createGraphicsPipeline_(device, shaderLoader, framebuffer);
}

void BlobRenderer2::execute(
        const vk::Device &device,
        const vk::CommandBuffer &commandBuffer,
        const Framebuffer &framebuffer,
        const Camera &camera,
        const blob::Model2 &blobModel
) {
    computeVertices(device, commandBuffer, blobModel);
    renderVertices(commandBuffer, framebuffer, camera, blobModel);
}

void BlobRenderer2::createComputePipeline_(const vk::Device &device, const ShaderLoader &shaderLoader) {
    UniqueCompiledShaderStage compShader = shaderLoader.loadFromSource(device, "resources/shaders/placeholder.comp");

    ComputePipelineConfig pipelineConfig = {
        .descriptorSetLayouts = {mComputeDescriptorLayout},
        .pushConstants = {vk::PushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = sizeof(ComputePushConstant),
        }},
    };

    mComputePipeline = createComputePipeline(device, pipelineConfig, *compShader);
}

void BlobRenderer2::createGraphicsPipeline_(
        const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer
) {
    UniqueCompiledShaderStage vertShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.vert");
    UniqueCompiledShaderStage fragShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.frag");

    GraphicsPipelineConfig pipelineConfig{};
    pipelineConfig.vertexInput = {blob::VertexData::getBindingDescriptions(), blob::VertexData::getAttributeDescriptions()};
    pipelineConfig.descriptorSetLayouts = {};
    pipelineConfig.pushConstants = {{vk::PushConstantRange{
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(VertexFragmentPushConstant)
    }}};
    pipelineConfig.attachments = {framebuffer.colorFormats(), framebuffer.depthFormat()};

    mGraphicsPipeline = createGraphicsPipeline(device, pipelineConfig, {*vertShader, *fragShader});
}

void BlobRenderer2::computeVertices(const vk::Device &device, const vk::CommandBuffer &commandBuffer, const blob::Model2 &blobModel) {
    const int resolution = blobModel.getResolution();

    mComputeDescriptors.next();
    DescriptorSet &set = mComputeDescriptors.get();

    /*
    vk::DescriptorBufferInfo sdfBufferInfo{
        .buffer = blobModel.getSdfSampleBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    */

    vk::DescriptorBufferInfo vertexBufferInfo{
        .buffer = blobModel.getVertexBuffer(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    auto writes = std::array{
        // set.write(ComputeDescriptorLayout::SDF_SAMPLES_BINDING, sdfBufferInfo),
        set.write(ComputeDescriptorLayout::VERTICES_BINDING, vertexBufferInfo),
    };

    device.updateDescriptorSets(writes, {});
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *mComputePipeline.pipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mComputePipeline.layout, 0, {set}, {});

    ComputePushConstant pc{};
    pc.resolution = resolution;

    commandBuffer.pushConstants(
            *mComputePipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstant), &pc
    );

    commandBuffer.dispatch(
            static_cast<uint32_t>(resolution), static_cast<uint32_t>(resolution),
            static_cast<uint32_t>(resolution)
    );
}

void BlobRenderer2::renderVertices(
        const vk::CommandBuffer &commandBuffer, const Framebuffer &framebuffer, const Camera &camera, const blob::Model2 &blobModel
) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *mGraphicsPipeline.pipeline);

    VertexFragmentPushConstant push{};
    glm::mat4 ModelMatrix = blobModel.getModelMatrix();
    push.projectionViewModel = camera.projectionMatrix() * camera.viewMatrix() * ModelMatrix;
    push.ModelMatrix = ModelMatrix;

    commandBuffer.pushConstants(
            *mGraphicsPipeline.layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(VertexFragmentPushConstant), &push
    );

    commandBuffer.beginRendering(framebuffer.renderingInfo({
        .enabledColorAttachments = {true},
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eLoad},
        .colorStoreOps = {vk::AttachmentStoreOp::eStore},
        .depthLoadOp = vk::AttachmentLoadOp::eLoad,
    }));

    mGraphicsPipeline.config.viewports = {{framebuffer.viewport(true)}};
    mGraphicsPipeline.config.scissors = {{framebuffer.area()}};
    mGraphicsPipeline.config.apply(commandBuffer);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *mGraphicsPipeline.pipeline);
    commandBuffer.bindVertexBuffers(0, {blobModel.getVertexBuffer()}, {0});
    commandBuffer.draw(/*blobModel.getVertexCount()*/ 3, 1, 0, 0); // TODO
    commandBuffer.endRendering();
}
