#include "Descriptors.h"

void DescriptorSetLayout::create(const vk::Device &device, vk::DescriptorSetLayoutCreateFlags flags, std::span<const Binding> bindings) {
    std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;
    std::vector<vk::DescriptorBindingFlags> binding_flags;

    layout_bindings.reserve(bindings.size());
    binding_flags.reserve(bindings.size());

    for (const auto& b : bindings) {
        layout_bindings.push_back(b);
        binding_flags.push_back(b.flags);
    }

    auto chain = vk::StructureChain{
        vk::DescriptorSetLayoutCreateInfo{}
        .setFlags(flags)
        .setBindings(layout_bindings),
        vk::DescriptorSetLayoutBindingFlagsCreateInfo{}
        .setBindingFlags(binding_flags)
    };

    mHandle = device.createDescriptorSetLayoutUnique(chain.get());
}

DescriptorAllocator::DescriptorAllocator(const vk::Device &device): mDevice(device) {
    std::vector<vk::DescriptorPoolSize> sizes = {
        {vk::DescriptorType::eCombinedImageSampler, 1024},
        {vk::DescriptorType::eUniformBuffer, 1024},
        {vk::DescriptorType::eStorageBuffer, 1024},
        {vk::DescriptorType::eStorageImage, 1024},
    };

    vk::DescriptorPoolInlineUniformBlockCreateInfo uniform_blocks = {
        .maxInlineUniformBlockBindings = 4096,
    };


    mPool = mDevice.createDescriptorPoolUnique({
        .pNext = &uniform_blocks,
        .maxSets = 1024,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    });
}

DescriptorSet DescriptorAllocator::allocate(const DescriptorSetLayout &layout) const {
    auto vk_layout = static_cast<vk::DescriptorSetLayout>(layout);
    vk::DescriptorSetAllocateInfo info = {
        .descriptorPool = *mPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vk_layout,
    };
    return DescriptorSet(mDevice.allocateDescriptorSets(info)[0]);
}
