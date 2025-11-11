#include "BlobRenderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../blob/model/VertexData.h"

void BlobRenderer::execute(
        const vk::CommandBuffer &commandBuffer, const Framebuffer &framebuffer, const Camera &camera, const blob::Model &blobModel
) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline.pipeline);

    BlobShaderPushConstant push{};
    glm::mat4 modelMatrix = blobModel.getModelMatrix();
    push.projectionViewModel = camera.projectionMatrix() * camera.viewMatrix() * modelMatrix;
    push.modelMatrix = modelMatrix;

    commandBuffer.pushConstants(
            *mPipeline.layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(BlobShaderPushConstant), &push
    );

    commandBuffer.beginRendering(framebuffer.renderingInfo({
        .enabledColorAttachments = {true},
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eLoad},
        .colorStoreOps = {vk::AttachmentStoreOp::eStore},
        .depthLoadOp = vk::AttachmentLoadOp::eLoad,
    }));

    mPipeline.config.viewports = {{framebuffer.viewport(true)}};
    mPipeline.config.scissors = {{framebuffer.area()}};
    mPipeline.config.apply(commandBuffer);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline.pipeline);
    commandBuffer.bindVertexBuffers(0, {blobModel.getVertexBuffer()}, {0});
    commandBuffer.draw(blobModel.getVertexCount(), 1, 0, 0);
    commandBuffer.endRendering();
}

void BlobRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
    UniqueCompiledShaderStage vertShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.vert");
    UniqueCompiledShaderStage fragShader = shaderLoader.loadFromSource(device, "resources/shaders/blob.frag");

    GraphicsPipelineConfig pipelineConfig{};
    pipelineConfig.vertexInput = {blob::VertexData::getBindingDescriptions(), blob::VertexData::getAttributeDescriptions()};
    pipelineConfig.descriptorSetLayouts = {};
    pipelineConfig.pushConstants = {{vk::PushConstantRange{
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(BlobShaderPushConstant)
    }}};
    pipelineConfig.attachments = {framebuffer.colorFormats(), framebuffer.depthFormat()};

    mPipeline = createGraphicsPipeline(device, pipelineConfig, {*vertShader, *fragShader});
}
