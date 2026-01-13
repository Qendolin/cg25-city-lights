#include "BloomRenderer.h"

#include "../backend/Image.h"
#include "../backend/ImageResource.h"
#include "../backend/ShaderCompiler.h"

BloomRenderer::BloomRenderer(const vk::Device &device) {

    mUpDescriptorLayout = UpDescriptorLayout(device);
    mUpSampler = device.createSamplerUnique({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
    });

    mDownDescriptorLayout = DownDescriptorLayout(device);
    mDownSampler = device.createSamplerUnique({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToBorder,
        .addressModeV = vk::SamplerAddressMode::eClampToBorder,
        .borderColor = vk::BorderColor::eFloatOpaqueBlack,
    });
}

const ImageViewBase &BloomRenderer::result() const { return mUpImageViews.at(0); }

void BloomRenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const ImageViewPairBase &hdr_attachment
) {
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mDownPipeline.pipeline);

    downPass(device, allocator, cmd_buf, hdr_attachment, ImageViewPair(mDownImage.get(), &mDownImageViews[0]), 0);

    for (int i = 1; i < LEVELS; i++) {
        downPass(
                device, allocator, cmd_buf, ImageViewPair(mDownImage.get(), &mDownImageViews[i - 1]),
                ImageViewPair(mDownImage.get(), &mDownImageViews[i]), i
        );
    }

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mUpPipeline.pipeline);

    ImageViewPair prev_image = {mDownImage.get(), &mDownImageViews[LEVELS - 1]};
    for (int i = LEVELS - 1; i >= 0; i--) {
        float prev_factor = i == LEVELS - 1 ? factors[i] : 1.0f;
        float curr_factor = i == 0 ? 0.0f : factors[i - 1];
        // curr and out have the same resolution
        // For i=0 we don't need a curr_image but something needs to be bound regardless
        ImageViewPair curr_image = ImageViewPair{mDownImage.get(), &mDownImageViews[std::max(i - 1, 0)]};
        ImageViewPair out_image = {mUpImage.get(), &mUpImageViews[i]};
        upPass(device, allocator, cmd_buf, prev_factor, prev_image, curr_factor, curr_image, out_image, i);
        prev_image = out_image;
    }

    barrier(ImageViewPair{mUpImage.get(), &mUpImageViews[0]}, mUpImageAccess, cmd_buf,
            ImageResourceAccess::ComputeShaderReadOptimal);
}

void BloomRenderer::downPass(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const ImageViewPairBase &in_image,
        const ImageViewPairBase &out_image,
        int level
) {
    util::ScopedCommandLabel dbg_cmd_label_region_culling(cmd_buf, "Down-Pass " + std::to_string(level));

    if (level == 0) {
        // level 0 gets hdr input
        in_image.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    } else {
        // Others get prev down level
        barrier(in_image, mDownImageAccess, cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    }

    barrier(out_image, mDownImageAccess, cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);

    auto descriptor_set = allocator.allocate(mDownDescriptorLayout);
    device.updateDescriptorSets(
            {
                descriptor_set.write(
                        DownDescriptorLayout::InColor,
                        vk::DescriptorImageInfo{
                            .sampler = *mDownSampler, .imageView = in_image.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set.write(
                        DownDescriptorLayout::OutColor,
                        vk::DescriptorImageInfo{.imageView = out_image.view(), .imageLayout = vk::ImageLayout::eGeneral}
                ),
            },
            {}
    );
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mDownPipeline.layout, 0, {descriptor_set}, {});

    DownPushConstants push_constants = {
        .thresholdCurve = glm::vec3(threshold - knee, knee * 2.0, 0.25 / knee), .threshold = threshold, .firstPass = level == 0
    };

    cmd_buf.pushConstants(*mDownPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);
    cmd_buf.dispatch(util::divCeil(out_image.view().info.width, 8u), util::divCeil(out_image.view().info.height, 8u), 1);
}

void BloomRenderer::upPass(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        float prev_factor,
        const ImageViewPairBase &in_prev_image,
        float curr_factor,
        const ImageViewPairBase &in_curr_image,
        const ImageViewPairBase &out_image,
        int level
) {
    util::ScopedCommandLabel dbg_cmd_label_region_culling(cmd_buf, "Up-Pass " + std::to_string(level));

    auto descriptor_set = allocator.allocate(mUpDescriptorLayout);

    // The first "previous" image is the last down sampled image
    auto &prev_tracker = level == LEVELS - 1 ? mDownImageAccess : mUpImageAccess;
    barrier(in_prev_image, prev_tracker, cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    barrier(in_curr_image, mDownImageAccess, cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    barrier(out_image, mUpImageAccess, cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);

    device.updateDescriptorSets(
            {
                descriptor_set.write(
                        UpDescriptorLayout::InCurrColor,
                        vk::DescriptorImageInfo{.imageView = in_curr_image.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal}
                ),
                descriptor_set.write(
                        UpDescriptorLayout::InPrevColor,
                        vk::DescriptorImageInfo{
                            .sampler = *mUpSampler, .imageView = in_prev_image.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set.write(
                        UpDescriptorLayout::OutColor,
                        vk::DescriptorImageInfo{.imageView = out_image.view(), .imageLayout = vk::ImageLayout::eGeneral}
                ),
            },
            {}
    );
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mUpPipeline.layout, 0, {descriptor_set}, {});

    UpPushConstants push_constants = {.prevFactor = prev_factor, .currFactor = curr_factor, .lastPass = level == 0};

    cmd_buf.pushConstants(*mUpPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);
    cmd_buf.dispatch(util::divCeil(out_image.view().info.width, 8u), util::divCeil(out_image.view().info.height, 8u), 1);
}

void BloomRenderer::barrier(
        const ImageViewPairBase &image,
        std::span<ImageResourceAccess> tracker,
        const vk::CommandBuffer &cmd_buf,
        const ImageResourceAccess &current
) {
    auto range = image.view().info.resourceRange;
    ImageResourceAccess &prev = tracker[range.baseMipLevel];
    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = prev.stage,
        .srcAccessMask = prev.access,
        .dstStageMask = current.stage,
        .dstAccessMask = current.access,
        .oldLayout = prev.layout,
        .newLayout = current.layout == vk::ImageLayout::eUndefined ? prev.layout : current.layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = range,
    };

    cmd_buf.pipelineBarrier2({
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    });

    prev = current;
}

void BloomRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader) {
    {
        auto comp_sh = shader_loader.loadFromSource(device, "resources/shaders/bloom_up.comp");

        ComputePipelineConfig pipeline_config = {
            .descriptorSetLayouts =
                    {
                        mUpDescriptorLayout,
                    },
            .pushConstants = {vk::PushConstantRange{
                .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(UpPushConstants)
            }}
        };

        mUpPipeline = createComputePipeline(device, pipeline_config, *comp_sh);
        util::setDebugName(device, *mUpPipeline.pipeline, "bloom_up");
    }

    {
        auto comp_sh = shader_loader.loadFromSource(device, "resources/shaders/bloom_down.comp");

        ComputePipelineConfig pipeline_config = {
            .descriptorSetLayouts =
                    {
                        mDownDescriptorLayout,
                    },
            .pushConstants = {vk::PushConstantRange{
                .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(DownPushConstants)
            }}
        };

        mDownPipeline = createComputePipeline(device, pipeline_config, *comp_sh);
        util::setDebugName(device, *mDownPipeline.pipeline, "bloom_down");
    }
}

void BloomRenderer::createImages(const vk::Device &device, const vma::Allocator &allocator, vk::Extent2D viewport_extent) {
    mUpImage = std::make_unique<Image>(std::move(Image::create(
            allocator,
            ImageCreateInfo{
                .format = vk::Format::eB10G11R11UfloatPack32,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .width = viewport_extent.width,
                .height = viewport_extent.height,
                .levels = LEVELS,
                .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            }
    )));
    util::setDebugName(device, *mUpImage->image, "bloom_up_image");

    vk::Extent2D view_extent = viewport_extent;
    mUpImageViews.resize(LEVELS);
    mUpImageAccess.clear();
    mUpImageAccess.resize(LEVELS);
    for (uint32_t i = 0; i < LEVELS; i++) {
        mUpImageViews[i] = ImageView::create(
                device, *mUpImage,
                ImageViewInfo{
                    .format = vk::Format::eB10G11R11UfloatPack32,
                    .width = view_extent.width,
                    .height = view_extent.height,
                    .resourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = i, .levelCount = 1, .layerCount = 1}
                }
        );
        util::setDebugName(device, *mUpImageViews[i].view, "bloom_up_image_view[" + std::to_string(i) + "]");
        view_extent.width = std::max(view_extent.width / 2, 1u);
        view_extent.height = std::max(view_extent.height / 2, 1u);
    }

    mDownImage = std::make_unique<Image>(std::move(Image::create(
            allocator,
            ImageCreateInfo{
                .format = vk::Format::eB10G11R11UfloatPack32,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .width = viewport_extent.width / 2,
                .height = viewport_extent.height / 2,
                .levels = LEVELS,
                .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
            }
    )));
    util::setDebugName(device, *mDownImage->image, "bloom_down_image");

    view_extent = {viewport_extent.width / 2, viewport_extent.height / 2};
    mDownImageViews.resize(LEVELS);
    mDownImageAccess.clear();
    mDownImageAccess.resize(LEVELS);
    for (uint32_t i = 0; i < LEVELS; i++) {
        mDownImageViews[i] = ImageView::create(
                device, *mDownImage,
                ImageViewInfo{
                    .format = vk::Format::eB10G11R11UfloatPack32,
                    .width = view_extent.width,
                    .height = view_extent.height,
                    .resourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = i, .levelCount = 1, .layerCount = 1}
                }
        );
        util::setDebugName(device, *mDownImageViews[i].view, "bloom_down_image_view[" + std::to_string(i) + "]");
        view_extent.width = std::max(view_extent.width / 2, 1u);
        view_extent.height = std::max(view_extent.height / 2, 1u);
    }
}
