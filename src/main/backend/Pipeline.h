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

    operator CompiledShaderStage() const { return {name, stage, *module}; }

    CompiledShaderStage operator*() const { return {name, stage, *module}; }
};

/// <summary>
/// Bitfield-like structure indicating which Vulkan dynamic states
/// are enabled or expected to be set dynamically.
/// </summary>
/// <remarks>
/// Each member corresponds to a Vulkan dynamic state as defined in the
/// Vulkan specification. When a flag is set to true, the corresponding
/// pipeline state is expected to be configured dynamically via a `vkCmdSet*`
/// command rather than being fixed in the pipeline.
/// </remarks>
struct DynamicStateFlags {
    /// <summary>
    /// VK_DYNAMIC_STATE_BLEND_CONSTANTS
    /// </summary>
    /// <remarks>
    /// Specifies that the blendConstants state in VkPipelineColorBlendStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetBlendConstants before
    /// any draws are performed with a pipeline state with VkPipelineColorBlendAttachmentState
    /// member blendEnable set to VK_TRUE and any of the blend functions using a constant blend color.
    /// </remarks>
    bool blendConstants = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT
    /// </summary>
    /// <remarks>
    /// Specifies that the blendEnable state in VkPipelineColorBlendAttachmentState
    /// will be ignored and must be set dynamically with vkCmdSetColorBlendEnableEXT
    /// before any draw call.
    /// </remarks>
    bool colorBlendEnable = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT
    /// </summary>
    /// <remarks>
    /// Specifies that the srcColorBlendFactor, dstColorBlendFactor, colorBlendOp,
    /// srcAlphaBlendFactor, dstAlphaBlendFactor, and alphaBlendOp states in
    /// VkPipelineColorBlendAttachmentState will be ignored and must be set dynamically
    /// with vkCmdSetColorBlendEquationEXT before any draw call.
    /// </remarks>
    bool colorBlendEquation = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT
    /// </summary>
    /// <remarks>
    /// Specifies that the colorWriteMask state in VkPipelineColorBlendAttachmentState
    /// will be ignored and must be set dynamically with vkCmdSetColorWriteMaskEXT
    /// before any draw call.
    /// </remarks>
    bool colorWriteMask = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_CULL_MODE
    /// </summary>
    /// <remarks>
    /// Specifies that the cullMode state in VkPipelineRasterizationStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetCullMode before
    /// any drawing commands.
    /// </remarks>
    bool cullMode = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_DEPTH_BIAS
    /// </summary>
    /// <remarks>
    /// Specifies that any instance of VkDepthBiasRepresentationInfoEXT included in
    /// the pNext chain of VkPipelineRasterizationStateCreateInfo as well as the
    /// depthBiasConstantFactor, depthBiasClamp and depthBiasSlopeFactor states
    /// in VkPipelineRasterizationStateCreateInfo will be ignored and must be set
    /// dynamically with vkCmdSetDepthBias or vkCmdSetDepthBias2EXT before any draws
    /// are performed with depth bias enabled.
    /// </remarks>
    bool depthBias = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE
    /// </summary>
    /// <remarks>
    /// Specifies that the depthBiasEnable state in VkPipelineRasterizationStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetDepthBiasEnable before
    /// any drawing commands.
    /// </remarks>
    bool depthBiasEnable = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT
    /// </summary>
    /// <remarks>
    /// Specifies that the depthClampEnable state in VkPipelineRasterizationStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetDepthClampEnableEXT before
    /// any draw call.
    /// </remarks>
    bool depthClampEnable = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_DEPTH_COMPARE_OP
    /// </summary>
    /// <remarks>
    /// Specifies that the depthCompareOp state in VkPipelineDepthStencilStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetDepthCompareOp before
    /// any draw call.
    /// </remarks>
    bool depthCompareOp = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
    /// </summary>
    /// <remarks>
    /// Specifies that the depthTestEnable state in VkPipelineDepthStencilStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetDepthTestEnable before
    /// any draw call.
    /// </remarks>
    bool depthTestEnable = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
    /// </summary>
    /// <remarks>
    /// Specifies that the depthWriteEnable state in VkPipelineDepthStencilStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetDepthWriteEnable before
    /// any draw call.
    /// </remarks>
    bool depthWriteEnable = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_FRONT_FACE
    /// </summary>
    /// <remarks>
    /// Specifies that the frontFace state in VkPipelineRasterizationStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetFrontFace before
    /// any drawing commands.
    /// </remarks>
    bool frontFace = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_LINE_WIDTH
    /// </summary>
    /// <remarks>
    /// Specifies that the lineWidth state in VkPipelineRasterizationStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetLineWidth before any
    /// drawing commands that generate line primitives for the rasterizer.
    /// </remarks>
    bool lineWidth = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_POLYGON_MODE_EXT
    /// </summary>
    /// <remarks>
    /// Specifies that the polygonMode state in VkPipelineRasterizationStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetPolygonModeEXT before
    /// any draw call.
    /// </remarks>
    bool polygonMode = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
    /// </summary>
    /// <remarks>
    /// Specifies that the scissorCount and pScissors state in VkPipelineViewportStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetScissorWithCount before any draw call.
    /// </remarks>
    bool scissor = true;

    /// <summary>
    /// VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
    /// </summary>
    /// <remarks>
    /// Specifies that the compareMask state in VkPipelineDepthStencilStateCreateInfo for both
    /// front and back will be ignored and must be set dynamically with vkCmdSetStencilCompareMask
    /// before any draws are performed with a pipeline state with
    /// VkPipelineDepthStencilStateCreateInfo member stencilTestEnable set to VK_TRUE.
    /// </remarks>
    bool stencilCompareMask = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_STENCIL_OP
    /// </summary>
    /// <remarks>
    /// Specifies that the failOp, passOp, depthFailOp, and compareOp states in
    /// VkPipelineDepthStencilStateCreateInfo for both front and back will be ignored
    /// and must be set dynamically with vkCmdSetStencilOp before any draws are performed
    /// with a pipeline state with VkPipelineDepthStencilStateCreateInfo member
    /// stencilTestEnable set to VK_TRUE.
    /// </remarks>
    bool stencilOp = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_STENCIL_REFERENCE
    /// </summary>
    /// <remarks>
    /// Specifies that the reference state in VkPipelineDepthStencilStateCreateInfo for both
    /// front and back will be ignored and must be set dynamically with vkCmdSetStencilReference
    /// before any draws are performed with a pipeline state with
    /// VkPipelineDepthStencilStateCreateInfo member stencilTestEnable set to VK_TRUE.
    /// </remarks>
    bool stencilReference = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE
    /// </summary>
    /// <remarks>
    /// Specifies that the stencilTestEnable state in VkPipelineDepthStencilStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetStencilTestEnable before
    /// any draw call.
    /// </remarks>
    bool stencilTestEnable = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
    /// </summary>
    /// <remarks>
    /// Specifies that the writeMask state in VkPipelineDepthStencilStateCreateInfo for both
    /// front and back will be ignored and must be set dynamically with vkCmdSetStencilWriteMask
    /// before any draws are performed with a pipeline state with
    /// VkPipelineDepthStencilStateCreateInfo member stencilTestEnable set to VK_TRUE.
    /// </remarks>
    bool stencilWriteMask = false;

    /// <summary>
    /// VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT
    /// </summary>
    /// <remarks>
    /// Specifies that the viewportCount and pViewports state in VkPipelineViewportStateCreateInfo
    /// will be ignored and must be set dynamically with vkCmdSetViewportWithCount before any draw call.
    /// </remarks>
    bool viewport = true;
};


/// <summary>
/// Configuration for creating a Vulkan graphics pipeline.
/// </summary>
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
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        }
    };

public:
    /// <summary>Describes the vertex input state.</summary>
    struct VertexInputInfo {
        /// <summary>Vertex input bindings.</summary>
        util::static_vector<vk::VertexInputBindingDescription, 16> bindings = {};
        /// <summary>Vertex input attributes.</summary>
        util::static_vector<vk::VertexInputAttributeDescription, 16> attributes = {};
    } vertexInput;

    /// <summary>Layouts of descriptor sets used by the pipeline.</summary>
    util::static_vector<vk::DescriptorSetLayout, 4> descriptorSetLayouts;
    /// <summary>Push constant ranges used by the pipeline.</summary>
    util::static_vector<vk::PushConstantRange, 32> pushConstants;

    /// <summary>Describes the primitive assembly state.</summary>
    struct PrimitiveAssemblyInfo {
        /// <summary>The primitive topology.</summary>
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        /// <summary>Whether primitive restart is enabled.</summary>
        bool restartEnabled = false;
    } primitiveAssembly;

    /// <summary>Describes the stencil test state.</summary>
    struct StencilInfo {
        /// <summary>Whether stencil testing is enabled.</summary>
        bool testEnabled = false;
        /// <summary>Stencil operations for front-facing polygons.</summary>
        vk::StencilOpState front = {};
        /// <summary>Stencil operations for back-facing polygons.</summary>
        vk::StencilOpState back = {};
    } stencil;

    /// <summary>Describes the depth test state.</summary>
    struct DepthInfo {
        /// <summary>Whether depth testing is enabled.</summary>
        bool testEnabled = true;
        /// <summary>Whether depth writes are enabled.</summary>
        bool writeEnabled = true;
        /// <summary>The depth comparison operator.</summary>
        vk::CompareOp compareOp = vk::CompareOp::eGreater;
        /// <summary>Whether depth bounds testing is enabled.</summary>
        bool boundsTest = false;
        /// <summary>The depth bounds for the depth bounds test.</summary>
        std::pair<float, float> bounds = {0.0f, 1.0f};
        /// <summary>Whether depth bias is enabled.</summary>
        bool biasEnabled = false;
        /// <summary>The depth bias parameters.</summary>
        vk::DepthBiasInfoEXT bias = {};
        /// <summary>Whether depth clamping is enabled.</summary>
        bool clampEnabled = true;
    } depth;

    /// <summary>Describes the formats of attachments used in the render pass.</summary>
    struct AttachmentsInfo {
        /// <summary>The formats of the color attachments.</summary>
        util::static_vector<vk::Format, 32> colorFormats;
        /// <summary>The format of the depth attachment.</summary>
        vk::Format depthFormat = vk::Format::eD32Sfloat;
        /// <summary>The format of the stencil attachment.</summary>
        vk::Format stencilFormat = vk::Format::eUndefined;
    } attachments;

    /// <summary>Describes the color blending state.</summary>
    struct BlendInfo {
        /// <summary>The per-attachment blend states.</summary>
        util::static_vector<vk::PipelineColorBlendAttachmentState, 32> state = DEFAULT_BLEND_STATE;
        /// <summary>The blend constants.</summary>
        std::array<float, 4> constants = {0, 0, 0, 0};
    } blend;

    /// <summary>Describes the rasterization state.</summary>
    struct RasterizerInfo {
        /// <summary>Whether primitives are discarded before rasterization.</summary>
        bool discardEnabled = false;
        /// <summary>The number of samples per pixel.</summary>
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
        /// <summary>The sample mask.</summary>
        util::static_vector<vk::SampleMask, 32> sampleMask = DEFAULT_SAMPLE_MASK;
        /// <summary>The polygon fill mode.</summary>
        vk::PolygonMode mode = vk::PolygonMode::eFill;
        /// <summary>Whether to enable alpha-to-coverage.</summary>
        bool alphaToCoverageEnabled = false;
    } rasterizer;

    /// <summary>Describes the culling state.</summary>
    struct CullInfo {
        /// <summary>The face culling mode.</summary>
        vk::CullModeFlagBits mode = vk::CullModeFlagBits::eBack;
        /// <summary>The front-face orientation.</summary>
        vk::FrontFace front = vk::FrontFace::eCounterClockwise;
    } cull;

    /// <summary>Describes the line rasterization state.</summary>
    struct LineInfo {
        /// <summary>The line width.</summary>
        float width = 1.0f;
        /// <summary>The line rasterization mode.</summary>
        vk::LineRasterizationModeEXT mode = vk::LineRasterizationModeEXT::eDefault;
        /// <summary>Whether line stippling is enabled.</summary>
        bool stippleEnabled = false;
        /// <summary>The line stipple factor.</summary>
        uint32_t stippleFactor = 0;
        /// <summary>The line stipple pattern.</summary>
        uint16_t stipplePattern = 0;
    } line;

    /// <summary>The viewports.</summary>
    util::static_vector<vk::Viewport, 8> viewports = {};
    /// <summary>The scissor rectangles.</summary>
    util::static_vector<vk::Rect2D, 8> scissors = {};

    /// <summary>Flags indicating which pipeline states are dynamic.</summary>
    DynamicStateFlags dynamic = {};

    /// <summary>Applies dynamic states to a command buffer.</summary>
    void apply(const vk::CommandBuffer &cmd) const;
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

ConfiguredPipeline createGraphicsPipeline(
        const vk::Device &device, const PipelineConfig &c, std::initializer_list<CompiledShaderStage> stages
);
