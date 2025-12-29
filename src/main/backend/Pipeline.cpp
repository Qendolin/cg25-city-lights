#include "Pipeline.h"

#include "../debug/Annotation.h"
#include "../glfw/Context.h"
#include "../util/Logger.h"
#include "../util/math.h"


SpecializationConstantsBuilder::SpecializationConstantsBuilder(size_t capacity)
    : mCapacity(capacity), mData(new std::byte[capacity]) {}

SpecializationConstantsBuilder& SpecializationConstantsBuilder::add(uint32_t id, size_t size, const void *data) {
    if (mOffset + size > mCapacity) {
        throw std::runtime_error("data size exceeds capacity");
    }
    memcpy(mData.get() + mOffset, data, size);
    mEntries.emplace_back(id, static_cast<uint32_t>(mOffset), size);
    mOffset += size;
    mOffset = util::alignOffset(mOffset, 4);
    return *this;
}

SpecializationConstants SpecializationConstantsBuilder::build() {
    SpecializationConstants result = {
        .entries = std::move(mEntries),
        .data = std::move(mData),
    };
    result.info = vk::SpecializationInfo{
        .mapEntryCount = static_cast<uint32_t>(result.entries.size()),
        .pMapEntries = result.entries.data(),
        .dataSize = mOffset,
        .pData = result.data.get(),
    };
    mOffset = 0;
    mCapacity = 0;
    return result;
}

ConfiguredGraphicsPipeline createGraphicsPipeline(
        const vk::Device &device,
        const GraphicsPipelineConfig &c,
        std::initializer_list<CompiledShaderStage> stages,
        std::initializer_list<std::reference_wrapper<const SpecializationConstants>> specializations
) {
    auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo()
                                      .setVertexAttributeDescriptions(c.vertexInput.attributes)
                                      .setVertexBindingDescriptions(c.vertexInput.bindings);
    auto input_assembly_state = vk::PipelineInputAssemblyStateCreateInfo{
        .topology = c.primitiveAssembly.topology,
        .primitiveRestartEnable = c.primitiveAssembly.restartEnabled,
    };

    auto rasterization_state = vk::PipelineRasterizationStateCreateInfo{
        .depthClampEnable = c.depth.clampEnabled,
        .rasterizerDiscardEnable = c.rasterizer.discardEnabled,
        .polygonMode = c.rasterizer.mode,
        .cullMode = c.cull.mode,
        .frontFace = c.cull.front,
        .depthBiasEnable = c.depth.biasEnabled,
        .depthBiasConstantFactor = c.depth.biasConstant,
        .depthBiasClamp = c.depth.biasClamp,
        .depthBiasSlopeFactor = c.depth.biasSlope,
        .lineWidth = c.line.width,
    };

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{
        .rasterizationSamples = c.rasterizer.samples,
        .pSampleMask = c.rasterizer.sampleMask.data(),
        .alphaToCoverageEnable = c.rasterizer.alphaToCoverageEnabled,
        .alphaToOneEnable = c.rasterizer.alphaToCoverageEnabled
    };

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = c.depth.testEnabled,
        .depthWriteEnable = c.depth.writeEnabled,
        .depthCompareOp = c.depth.compareOp,
        .depthBoundsTestEnable = c.depth.boundsTest,
        .stencilTestEnable = c.stencil.testEnabled,
        .front = c.stencil.front,
        .back = c.stencil.back,
        .minDepthBounds = c.depth.bounds.first,
        .maxDepthBounds = c.depth.bounds.second,
    };

    auto color_blend_state =
            vk::PipelineColorBlendStateCreateInfo{
                .logicOpEnable = false,
                .blendConstants = c.blend.constants,
            }
                    .setAttachments(c.blend.state);

    auto viewport_state = vk::PipelineViewportStateCreateInfo().setViewports(c.viewports).setScissors(c.scissors);


    auto layout = device.createPipelineLayoutUnique(
            vk::PipelineLayoutCreateInfo().setSetLayouts(c.descriptorSetLayouts).setPushConstantRanges(c.pushConstants)
    );

    std::vector<vk::DynamicState> dynamic_states = {};
    dynamic_states.reserve(64);

    DynamicStateFlags dynamic_state_flags = c.dynamic;

    if (dynamic_state_flags.blendConstants)
        dynamic_states.push_back(vk::DynamicState::eBlendConstants);
    if (dynamic_state_flags.colorBlendEnable)
        dynamic_states.push_back(vk::DynamicState::eColorBlendEnableEXT);
    if (dynamic_state_flags.colorBlendEquation)
        dynamic_states.push_back(vk::DynamicState::eColorBlendEquationEXT);
    if (dynamic_state_flags.colorWriteMask)
        dynamic_states.push_back(vk::DynamicState::eColorWriteMaskEXT);
    if (dynamic_state_flags.cullMode)
        dynamic_states.push_back(vk::DynamicState::eCullMode);
    if (dynamic_state_flags.depthBias)
        dynamic_states.push_back(vk::DynamicState::eDepthBias);
    if (dynamic_state_flags.depthBiasEnable)
        dynamic_states.push_back(vk::DynamicState::eDepthBiasEnable);
    if (dynamic_state_flags.depthClampEnable)
        dynamic_states.push_back(vk::DynamicState::eDepthClampEnableEXT);
    if (dynamic_state_flags.depthCompareOp)
        dynamic_states.push_back(vk::DynamicState::eDepthCompareOp);
    if (dynamic_state_flags.depthTestEnable)
        dynamic_states.push_back(vk::DynamicState::eDepthTestEnable);
    if (dynamic_state_flags.depthWriteEnable)
        dynamic_states.push_back(vk::DynamicState::eDepthWriteEnable);
    if (dynamic_state_flags.frontFace)
        dynamic_states.push_back(vk::DynamicState::eFrontFace);
    if (dynamic_state_flags.lineWidth)
        dynamic_states.push_back(vk::DynamicState::eLineWidth);
    if (dynamic_state_flags.polygonMode)
        dynamic_states.push_back(vk::DynamicState::ePolygonModeEXT);
    if (dynamic_state_flags.scissor)
        dynamic_states.push_back(vk::DynamicState::eScissorWithCount);
    if (dynamic_state_flags.stencilCompareMask)
        dynamic_states.push_back(vk::DynamicState::eStencilCompareMask);
    if (dynamic_state_flags.stencilOp)
        dynamic_states.push_back(vk::DynamicState::eStencilOp);
    if (dynamic_state_flags.stencilReference)
        dynamic_states.push_back(vk::DynamicState::eStencilReference);
    if (dynamic_state_flags.stencilTestEnable)
        dynamic_states.push_back(vk::DynamicState::eStencilTestEnable);
    if (dynamic_state_flags.stencilWriteMask)
        dynamic_states.push_back(vk::DynamicState::eStencilWriteMask);
    if (dynamic_state_flags.viewport)
        dynamic_states.push_back(vk::DynamicState::eViewportWithCount);

    auto dynamic_state = vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamic_states);

    vk::ShaderStageFlags stage_flags = {};
    std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_create_infos;
    for (const auto &stage: stages) {
        stage_flags |= stage.stage;
        shader_stage_create_infos.emplace_back() = vk::PipelineShaderStageCreateInfo{
            .stage = stage.stage,
            .module = stage.module,
            .pName = "main",
        };
    }

    for (size_t i = 0; i < specializations.size(); i++) {
        const auto &value = *(specializations.begin() + i);
        shader_stage_create_infos.at(i).pSpecializationInfo = &value.get().info;
    }

    auto pipeline_create_info =
            vk::GraphicsPipelineCreateInfo{
                .pVertexInputState = &vertex_input_state,
                .pInputAssemblyState = &input_assembly_state,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterization_state,
                .pMultisampleState = &multisample_state,
                .pDepthStencilState = &depth_stencil_state,
                .pColorBlendState = &color_blend_state,
                .pDynamicState = &dynamic_state,
                .layout = *layout,
            }
                    .setStages(shader_stage_create_infos);

    auto pipeline_rendering_create_info = vk::PipelineRenderingCreateInfo()
                                                  .setColorAttachmentFormats(c.attachments.colorFormats)
                                                  .setDepthAttachmentFormat(c.attachments.depthFormat)
                                                  .setStencilAttachmentFormat(c.attachments.stencilFormat);

    pipeline_create_info.pNext = &pipeline_rendering_create_info;

    auto pipeline = device.createGraphicsPipelineUnique(nullptr, pipeline_create_info);

    return {
        .stages = stage_flags,
        .layout = std::move(layout),
        .pipeline = std::move(pipeline.value),
        .config = c,
    };
}

ConfiguredComputePipeline createComputePipeline(
        const vk::Device &device,
        const ComputePipelineConfig &c,
        const CompiledShaderStage &shader,
        const SpecializationConstants &specialization
) {

    vk::PipelineShaderStageCreateInfo shader_stage_create_info = {
        .stage = shader.stage,
        .module = shader.module,
        .pName = "main",
        .pSpecializationInfo = &specialization.info,
    };

    auto layout = device.createPipelineLayoutUnique(
            vk::PipelineLayoutCreateInfo().setSetLayouts(c.descriptorSetLayouts).setPushConstantRanges(c.pushConstants)
    );

    vk::ComputePipelineCreateInfo pipeline_create_info = {
        .stage = shader_stage_create_info,
        .layout = *layout,
    };

    auto pipeline = device.createComputePipelineUnique(nullptr, pipeline_create_info);

    return {
        .layout = std::move(layout),
        .pipeline = std::move(pipeline.value),
        .config = c,
    };
}

void GraphicsPipelineConfig::apply(const vk::CommandBuffer &cmd) const {
    const DynamicStateFlags &flags = dynamic;
    if (flags.blendConstants)
        cmd.setBlendConstants(blend.constants.data());

    if (flags.colorBlendEnable) {
        Logger::check(!blend.state.empty(), "No blend states in pipeline config!");
        util::static_vector<vk::Bool32, std::extent_v<decltype(blend.state), 1>> values;
        for (size_t i = 0; i < blend.state.size(); i++) {
            values.push_back(blend.state[i].blendEnable);
        }
        cmd.setColorBlendEnableEXT(0, values);
    }
    if (flags.colorBlendEquation) {
        Logger::check(!blend.state.empty(), "No blend states in pipeline config!");
        util::static_vector<vk::ColorBlendEquationEXT, std::extent_v<decltype(blend.state), 1>> values;
        for (size_t i = 0; i < blend.state.size(); i++) {
            const auto &state = blend.state[i];
            values.emplace_back() = vk::ColorBlendEquationEXT{
                .srcColorBlendFactor = state.srcColorBlendFactor,
                .dstColorBlendFactor = state.dstColorBlendFactor,
                .colorBlendOp = state.colorBlendOp,
                .srcAlphaBlendFactor = state.srcAlphaBlendFactor,
                .dstAlphaBlendFactor = state.dstAlphaBlendFactor,
                .alphaBlendOp = state.alphaBlendOp,
            };
        }
        cmd.setColorBlendEquationEXT(0, values);
    }
    if (flags.colorWriteMask) {
        Logger::check(!blend.state.empty(), "No blend states in pipeline config!");
        util::static_vector<vk::ColorComponentFlags, std::extent_v<decltype(blend.state), 1>> values;
        for (size_t i = 0; i < blend.state.size(); i++) {
            values.push_back(blend.state[i].colorWriteMask);
        }
        cmd.setColorWriteMaskEXT(0, values);
    }
    if (flags.cullMode)
        cmd.setCullMode(cull.mode);

    if (flags.depthBias)
        cmd.setDepthBias(depth.biasConstant, depth.biasClamp, depth.biasSlope);

    if (flags.depthBiasEnable)
        cmd.setDepthBiasEnable(depth.biasEnabled);

    if (flags.depthClampEnable)
        cmd.setDepthClampEnableEXT(depth.clampEnabled);

    if (flags.depthCompareOp)
        cmd.setDepthCompareOp(depth.compareOp);

    if (flags.depthTestEnable)
        cmd.setDepthTestEnable(depth.testEnabled);

    if (flags.depthWriteEnable)
        cmd.setDepthWriteEnableEXT(depth.writeEnabled);

    if (flags.frontFace)
        cmd.setFrontFace(cull.front);

    if (flags.lineWidth)
        cmd.setLineWidth(line.width);

    if (flags.polygonMode)
        cmd.setPolygonModeEXT(rasterizer.mode);

    if (flags.scissor) {
        Logger::check(!scissors.empty(), "No scissor regions in pipeline config!");
        cmd.setScissorWithCount(scissors);
    }

    if (flags.stencilCompareMask) {
        cmd.setStencilCompareMask(vk::StencilFaceFlagBits::eFront, stencil.front.compareMask);
        cmd.setStencilCompareMask(vk::StencilFaceFlagBits::eBack, stencil.back.compareMask);
    }
    if (flags.stencilOp) {
        cmd.setStencilOp(
                vk::StencilFaceFlagBits::eFront, stencil.front.failOp, stencil.front.passOp, stencil.front.depthFailOp,
                stencil.front.compareOp
        );
        cmd.setStencilOp(
                vk::StencilFaceFlagBits::eBack, stencil.back.failOp, stencil.back.passOp, stencil.back.depthFailOp,
                stencil.back.compareOp
        );
    }
    if (flags.stencilReference) {
        cmd.setStencilReference(vk::StencilFaceFlagBits::eFront, stencil.front.reference);
        cmd.setStencilReference(vk::StencilFaceFlagBits::eBack, stencil.back.reference);
    }
    if (flags.stencilTestEnable)
        cmd.setStencilTestEnable(stencil.testEnabled);

    if (flags.stencilWriteMask) {
        cmd.setStencilWriteMask(vk::StencilFaceFlagBits::eFront, stencil.front.writeMask);
        cmd.setStencilWriteMask(vk::StencilFaceFlagBits::eBack, stencil.back.writeMask);
    }
    if (flags.viewport) {
        Logger::check(!viewports.empty(), "No viewports in pipeline config!");
        cmd.setViewportWithCount(viewports);
    }
}
