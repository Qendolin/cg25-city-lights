#include "SkyboxRenderer.h"

#include "../backend/ShaderCompiler.h"
#include "../util/globals.h"

SkyboxRenderer::SkyboxRenderer(const vk::Device &device, const DescriptorAllocator &allocator) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);

    mShaderParamsDescriptors.create(util::MaxFramesInFlight, [&]() {
        return allocator.allocate(mShaderParamsDescriptorLayout);
    });

    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.anisotropyEnable = false;
    samplerInfo.compareEnable = false;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = false;

    mSampler = device.createSamplerUnique(samplerInfo);
}

SkyboxRenderer::~SkyboxRenderer() {}

void SkyboxRenderer::execute(
        const vk::Device &device,
        const vk::CommandBuffer &commandBuffer,
        const Framebuffer &framebuffer,
        const Camera &camera,
        const Cubemap &skybox
) {
    commandBuffer.beginRendering(framebuffer.renderingInfo({
        .enabledColorAttachments = {true},
        .enableDepthAttachment = false,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eClear},
        .colorStoreOps = {vk::AttachmentStoreOp::eStore}
    }));

    mPipeline.config.viewports = {{framebuffer.viewport(true)}};
    mPipeline.config.scissors = {{framebuffer.area()}};
    mPipeline.config.apply(commandBuffer);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline.pipeline);

    ShaderParamsPushConstants push{};
    push.projViewNoTranslation = camera.projectionMatrix() * glm::mat4(glm::mat3(camera.viewMatrix()));

    commandBuffer.pushConstants(
            *mPipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(ShaderParamsPushConstants), &push
    );

    vk::DescriptorImageInfo descriptorImageInfo{};
    descriptorImageInfo.sampler = *mSampler;
    descriptorImageInfo.imageView = skybox.getImageView();
    descriptorImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    mShaderParamsDescriptors.next();

    device.updateDescriptorSets(
            {mShaderParamsDescriptors.get().write(ShaderParamsDescriptorLayout::SamplerCubeMap, descriptorImageInfo)}, {}
    );

    commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *mPipeline.layout, 0, {mShaderParamsDescriptors.get()}, {}
    );

    commandBuffer.draw(SKYBOX_VERTEX_COUNT, 1, 0, 0);
    commandBuffer.endRendering();
}

void SkyboxRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
    UniqueCompiledShaderStage vertShader = shaderLoader.loadFromSource(device, "resources/shaders/skybox.vert");
    UniqueCompiledShaderStage fragShader = shaderLoader.loadFromSource(device, "resources/shaders/skybox.frag");

    vk::PushConstantRange pushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(ShaderParamsPushConstants)};

    GraphicsPipelineConfig pipelineConfig{};
    pipelineConfig.descriptorSetLayouts = {mShaderParamsDescriptorLayout};
    pipelineConfig.pushConstants = {pushConstantRange};
    pipelineConfig.attachments = {framebuffer.colorFormats(), framebuffer.depthFormat()};
    pipelineConfig.cull.mode = vk::CullModeFlagBits::eNone;

    pipelineConfig.depth.testEnabled = false;
    pipelineConfig.depth.writeEnabled = false;
    pipelineConfig.depth.compareOp = vk::CompareOp::eLessOrEqual;

    mPipeline = createGraphicsPipeline(device, pipelineConfig, {*vertShader, *fragShader});
}
