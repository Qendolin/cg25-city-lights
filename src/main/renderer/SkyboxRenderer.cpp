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

    vk::DescriptorImageInfo descriptor_image_info{};
    descriptor_image_info.sampler = *mSampler;
    descriptor_image_info.imageView = skybox.getImageView();
    descriptor_image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    auto descriptor_set = allocator.allocate(mShaderParamsDescriptorLayout);

    device.updateDescriptorSets(
            {descriptor_set.write(ShaderParamsDescriptorLayout::SamplerCubeMap, descriptor_image_info)}, {}
    );

    cmd_buf.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *mPipeline.layout, 0, {descriptor_set}, {}
    );

    cmd_buf.draw(SKYBOX_VERTEX_COUNT, 1, 0, 0);
    cmd_buf.endRendering();
}

void SkyboxRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &fb) {
    UniqueCompiledShaderStage vert_shader = shaderLoader.loadFromSource(device, "resources/shaders/skybox.vert");
    UniqueCompiledShaderStage frag_shader = shaderLoader.loadFromSource(device, "resources/shaders/skybox.frag");

    vk::PushConstantRange push_constant_range{vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(ShaderParamsPushConstants)};

    GraphicsPipelineConfig pipeline_config{};
    pipeline_config.descriptorSetLayouts = {mShaderParamsDescriptorLayout};
    pipeline_config.pushConstants = {push_constant_range};
    pipeline_config.attachments = {fb.colorFormats(), fb.depthFormat()};
    pipeline_config.cull.mode = vk::CullModeFlagBits::eNone;

    pipeline_config.depth.testEnabled = true;
    pipeline_config.depth.writeEnabled = false;
    pipeline_config.depth.compareOp = vk::CompareOp::eGreaterOrEqual;

    pipeline_config.rasterizer.samples = fb.depthAttachment.image().info.samples;

    mPipeline = createGraphicsPipeline(device, pipeline_config, {*vert_shader, *frag_shader});
    util::setDebugName(device, *mPipeline.pipeline, "skybox");
}
