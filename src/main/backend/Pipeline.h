#pragma once

#include <vulkan/vulkan.hpp>

#include "../util/static_vector.h"


struct CompiledShaderStage {
    std::string name;
    vk::ShaderStageFlagBits stage;
    vk::ShaderModule module;
};

struct UniqueCompiledShaderStage {
    std::string name;
    vk::ShaderStageFlagBits stage;
    vk::UniqueShaderModule module;

    operator CompiledShaderStage() const {
        return {name, stage, *module};
    }

    CompiledShaderStage operator *() const {
        return {name, stage, *module};
    }
};

/**
 * @brief Bitfield-like structure indicating which Vulkan dynamic states
 *        are enabled or expected to be set dynamically.
 *
 * Each member corresponds to a Vulkan dynamic state as defined in the
 * Vulkan specification. When a flag is set to true, the corresponding
 * pipeline state is expected to be configured dynamically via a `vkCmdSet*`
 * command rather than being fixed in the pipeline.
 */
struct DynamicStateFlags {
    /**
     * @brief VK_DYNAMIC_STATE_BLEND_CONSTANTS
     *
     * Specifies that the blendConstants state in VkPipelineColorBlendStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetBlendConstants before
     * any draws are performed with a pipeline state with VkPipelineColorBlendAttachmentState
     * member blendEnable set to VK_TRUE and any of the blend functions using a constant blend color.
     */
    bool blendConstants = false;

    /**
     * @brief VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT
     *
     * Specifies that the blendEnable state in VkPipelineColorBlendAttachmentState
     * will be ignored and must be set dynamically with vkCmdSetColorBlendEnableEXT
     * before any draw call.
     */
    bool colorBlendEnable = false;

    /**
     * @brief VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT
     *
     * Specifies that the srcColorBlendFactor, dstColorBlendFactor, colorBlendOp,
     * srcAlphaBlendFactor, dstAlphaBlendFactor, and alphaBlendOp states in
     * VkPipelineColorBlendAttachmentState will be ignored and must be set dynamically
     * with vkCmdSetColorBlendEquationEXT before any draw call.
     */
    bool colorBlendEquation = false;

    /**
     * @brief VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT
     *
     * Specifies that the colorWriteMask state in VkPipelineColorBlendAttachmentState
     * will be ignored and must be set dynamically with vkCmdSetColorWriteMaskEXT
     * before any draw call.
     */
    bool colorWriteMask = false;

    /**
     * @brief VK_DYNAMIC_STATE_CULL_MODE
     *
     * Specifies that the cullMode state in VkPipelineRasterizationStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetCullMode before
     * any drawing commands.
     */
    bool cullMode = false;

    /**
     * @brief VK_DYNAMIC_STATE_DEPTH_BIAS
     *
     * Specifies that any instance of VkDepthBiasRepresentationInfoEXT included in
     * the pNext chain of VkPipelineRasterizationStateCreateInfo as well as the
     * depthBiasConstantFactor, depthBiasClamp and depthBiasSlopeFactor states
     * in VkPipelineRasterizationStateCreateInfo will be ignored and must be set
     * dynamically with vkCmdSetDepthBias or vkCmdSetDepthBias2EXT before any draws
     * are performed with depth bias enabled.
     */
    bool depthBias = false;

    /**
     * @brief VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE
     *
     * Specifies that the depthBiasEnable state in VkPipelineRasterizationStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetDepthBiasEnable before
     * any drawing commands.
     */
    bool depthBiasEnable = false;

    /**
     * @brief VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT
     *
     * Specifies that the depthClampEnable state in VkPipelineRasterizationStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetDepthClampEnableEXT before
     * any draw call.
     */
    bool depthClampEnable = false;

    /**
     * @brief VK_DYNAMIC_STATE_DEPTH_COMPARE_OP
     *
     * Specifies that the depthCompareOp state in VkPipelineDepthStencilStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetDepthCompareOp before
     * any draw call.
     */
    bool depthCompareOp = false;

    /**
     * @brief VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
     *
     * Specifies that the depthTestEnable state in VkPipelineDepthStencilStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetDepthTestEnable before
     * any draw call.
     */
    bool depthTestEnable = false;

    /**
     * @brief VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
     *
     * Specifies that the depthWriteEnable state in VkPipelineDepthStencilStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetDepthWriteEnable before
     * any draw call.
     */
    bool depthWriteEnable = false;

    /**
     * @brief VK_DYNAMIC_STATE_FRONT_FACE
     *
     * Specifies that the frontFace state in VkPipelineRasterizationStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetFrontFace before
     * any drawing commands.
     */
    bool frontFace = false;

    /**
     * @brief VK_DYNAMIC_STATE_LINE_WIDTH
     *
     * Specifies that the lineWidth state in VkPipelineRasterizationStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetLineWidth before any
     * drawing commands that generate line primitives for the rasterizer.
     */
    bool lineWidth = false;

    /**
     * @brief VK_DYNAMIC_STATE_POLYGON_MODE_EXT
     *
     * Specifies that the polygonMode state in VkPipelineRasterizationStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetPolygonModeEXT before
     * any draw call.
     */
    bool polygonMode = false;

    /**
     * @brief VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
     *
     * Specifies that the scissorCount and pScissors state in VkPipelineViewportStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetScissorWithCount before any draw call.
     */
    bool scissor = true;

    /**
     * @brief VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
     *
     * Specifies that the compareMask state in VkPipelineDepthStencilStateCreateInfo for both
     * front and back will be ignored and must be set dynamically with vkCmdSetStencilCompareMask
     * before any draws are performed with a pipeline state with
     * VkPipelineDepthStencilStateCreateInfo member stencilTestEnable set to VK_TRUE.
     */
    bool stencilCompareMask = false;

    /**
     * @brief VK_DYNAMIC_STATE_STENCIL_OP
     *
     * Specifies that the failOp, passOp, depthFailOp, and compareOp states in
     * VkPipelineDepthStencilStateCreateInfo for both front and back will be ignored
     * and must be set dynamically with vkCmdSetStencilOp before any draws are performed
     * with a pipeline state with VkPipelineDepthStencilStateCreateInfo member
     * stencilTestEnable set to VK_TRUE.
     */
    bool stencilOp = false;

    /**
     * @brief VK_DYNAMIC_STATE_STENCIL_REFERENCE
     *
     * Specifies that the reference state in VkPipelineDepthStencilStateCreateInfo for both
     * front and back will be ignored and must be set dynamically with vkCmdSetStencilReference
     * before any draws are performed with a pipeline state with
     * VkPipelineDepthStencilStateCreateInfo member stencilTestEnable set to VK_TRUE.
     */
    bool stencilReference = false;

    /**
     * @brief VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE
     *
     * Specifies that the stencilTestEnable state in VkPipelineDepthStencilStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetStencilTestEnable before
     * any draw call.
     */
    bool stencilTestEnable = false;

    /**
     * @brief VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
     *
     * Specifies that the writeMask state in VkPipelineDepthStencilStateCreateInfo for both
     * front and back will be ignored and must be set dynamically with vkCmdSetStencilWriteMask
     * before any draws are performed with a pipeline state with
     * VkPipelineDepthStencilStateCreateInfo member stencilTestEnable set to VK_TRUE.
     */
    bool stencilWriteMask = false;

    /**
     * @brief VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT
     *
     * Specifies that the viewportCount and pViewports state in VkPipelineViewportStateCreateInfo
     * will be ignored and must be set dynamically with vkCmdSetViewportWithCount before any draw call.
     */
    bool viewport = true;
};


struct PipelineConfig {
private:
    static constexpr std::array<uint32_t, 1> DEFAULT_SAMPLE_MASK = {UINT32_MAX};
    static constexpr std::array<vk::PipelineColorBlendAttachmentState, 1> DEFAULT_BLEND_STATE = {
        vk::PipelineColorBlendAttachmentState{
            .blendEnable = false,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                              vk::ColorComponentFlagBits::eA,
        }
    };

public:
    struct VertexInputInfo {
        util::static_vector<vk::VertexInputBindingDescription, 16> bindings = {};
        util::static_vector<vk::VertexInputAttributeDescription, 16> attributes = {};
    } vertexInput;

    util::static_vector<vk::DescriptorSetLayout, 4> descriptorSetLayouts;
    util::static_vector<vk::PushConstantRange, 32> pushConstants;

    struct PrimitiveAssemblyInfo {
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        bool restartEnabled = false;
    } primitiveAssembly;

    struct StencilInfo {
        bool testEnabled = false;
        vk::StencilOpState front = {};
        vk::StencilOpState back = {};
    } stencil;

    struct DepthInfo {
        bool testEnabled = true;
        bool writeEnabled = true;
        vk::CompareOp compareOp = vk::CompareOp::eLess;
        bool boundsTest = false;
        std::pair<float, float> bounds = {0.0f, 1.0f};
        bool biasEnabled = false;
        vk::DepthBiasInfoEXT bias = {};
        bool clampEnabled = true;
    } depth;

    struct BlendInfo {
        util::static_vector<vk::PipelineColorBlendAttachmentState, 32> state = DEFAULT_BLEND_STATE;
        std::array<float, 4> constants = {0, 0, 0, 0};
    } blend;

    struct RasterizerInfo {
        bool discardEnabled = false;
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
        util::static_vector<vk::SampleMask, 32> sampleMask = DEFAULT_SAMPLE_MASK;
        vk::PolygonMode mode = vk::PolygonMode::eFill;
        bool alphaToCoverageEnabled = false;
    } rasterizer;

    struct CullInfo {
        vk::CullModeFlagBits mode = vk::CullModeFlagBits::eBack;
        vk::FrontFace front = vk::FrontFace::eCounterClockwise;
    } cull;

    struct LineInfo {
        float width = 1.0f;
        vk::LineRasterizationModeEXT mode = vk::LineRasterizationModeEXT::eDefault;
        bool stippleEnabled = false;
        uint32_t stippleFactor = 0;
        uint16_t stipplePattern = 0;
    } line;

    util::static_vector<vk::Viewport, 8> viewports = {};
    util::static_vector<vk::Rect2D, 8> scissors = {};

    DynamicStateFlags dynamic = {};

    void apply(const vk::CommandBuffer& cmd) const;
};

struct Pipeline {
    vk::ShaderStageFlags stages = {};
    vk::PipelineLayout layout = {};
    vk::Pipeline pipeline = {};
};

struct ConfiguredPipeline {
    vk::ShaderStageFlags stages = {};
    vk::UniquePipelineLayout layout = {};
    vk::UniquePipeline pipeline = {};
    PipelineConfig config = {};
};

ConfiguredPipeline createGraphicsPipeline(const vk::Device& device, const PipelineConfig &c, std::initializer_list<CompiledShaderStage> stages);
