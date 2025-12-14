#include "SkyboxRenderer.h"

#include "../backend/ShaderCompiler.h"
#include "../debug/Annotation.h"

SkyboxRenderer::SkyboxRenderer(const vk::Device &device) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);

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
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const Framebuffer &framebuffer,
        const Camera &camera,
        const Cubemap &skybox,
        float exposure,
        const glm::vec3& tint
) {
    cmd_buf.beginRendering(framebuffer.renderingInfo({
        .enableColorAttachments = true,
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eLoad},
        .colorStoreOps = {vk::AttachmentStoreOp::eStore},
        .depthLoadOp = vk::AttachmentLoadOp::eLoad,
        .depthStoreOp = vk::AttachmentStoreOp::eNone,
    }));

    mPipeline.config.viewports = {{framebuffer.viewport(true)}};
    mPipeline.config.scissors = {{framebuffer.area()}};
    mPipeline.config.apply(cmd_buf);

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline.pipeline);

    ShaderParamsPushConstants push{};
    push.projViewNoTranslation = camera.projectionMatrix() * glm::mat4(glm::mat3(camera.viewMatrix()));
    push.tint = glm::vec4(tint, 1.0f) * exp2(exposure);

    cmd_buf.pushConstants(
            *mPipeline.layout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(ShaderParamsPushConstants), &push
    );

    vk::DescriptorImageInfo descriptorImageInfo{};
    descriptorImageInfo.sampler = *mSampler;
    descriptorImageInfo.imageView = skybox.getImageView();
    descriptorImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    auto descriptor_set = allocator.allocate(mShaderParamsDescriptorLayout);

    device.updateDescriptorSets(
            {descriptor_set.write(ShaderParamsDescriptorLayout::SamplerCubeMap, descriptorImageInfo)}, {}
    );

    cmd_buf.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *mPipeline.layout, 0, {descriptor_set}, {}
    );

    cmd_buf.draw(SKYBOX_VERTEX_COUNT, 1, 0, 0);
    cmd_buf.endRendering();
}

void SkyboxRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
    UniqueCompiledShaderStage vertShader = shaderLoader.loadFromSource(device, "resources/shaders/skybox.vert");
    UniqueCompiledShaderStage fragShader = shaderLoader.loadFromSource(device, "resources/shaders/skybox.frag");

    vk::PushConstantRange pushConstantRange{vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(ShaderParamsPushConstants)};

    GraphicsPipelineConfig pipelineConfig{};
    pipelineConfig.descriptorSetLayouts = {mShaderParamsDescriptorLayout};
    pipelineConfig.pushConstants = {pushConstantRange};
    pipelineConfig.attachments = {framebuffer.colorFormats(), framebuffer.depthFormat()};
    pipelineConfig.cull.mode = vk::CullModeFlagBits::eNone;

    pipelineConfig.depth.testEnabled = true;
    pipelineConfig.depth.writeEnabled = false;
    pipelineConfig.depth.compareOp = vk::CompareOp::eGreaterOrEqual;

    mPipeline = createGraphicsPipeline(device, pipelineConfig, {*vertShader, *fragShader});
}
