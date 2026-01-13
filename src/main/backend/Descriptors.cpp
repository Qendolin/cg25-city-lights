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

struct DescriptorAllocatorImpl {
    vk::Device mDevice;
    vk::DescriptorPool mCurrentPool;
    std::vector<vk::DescriptorPool> mUsedPools;

    explicit DescriptorAllocatorImpl(vk::Device device) : mDevice(device) {}

    [[nodiscard]] vk::DescriptorPool createPool() const {
        std::array sizes = {
            vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, 1024},
            vk::DescriptorPoolSize{vk::DescriptorType::eInlineUniformBlock, 1024}
        };

        vk::DescriptorPoolInlineUniformBlockCreateInfo inlineUniformInfo{};
        inlineUniformInfo.maxInlineUniformBlockBindings = 1024;

        vk::DescriptorPoolCreateInfo info{};
        info.maxSets = 1024;
        info.poolSizeCount = static_cast<uint32_t>(sizes.size());
        info.pPoolSizes = sizes.data();
        info.pNext = &inlineUniformInfo;

        return mDevice.createDescriptorPool(info);
    }
};

DescriptorSet DescriptorAllocator::allocate(const vk::DescriptorSetLayout& layout) const {
    assert(mImpl);

    // Initialize current pool if null
    if (!mImpl->mCurrentPool) {
        mImpl->mCurrentPool = mImpl->createPool();
    }

    vk::DescriptorSetAllocateInfo allocInfo = {};
    allocInfo.descriptorPool = mImpl->mCurrentPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    // Try to allocate. If it fails due to pool limits, grow and retry.
    try {
        return DescriptorSet(mImpl->mDevice.allocateDescriptorSets(allocInfo)[0]);
    }
    catch (vk::SystemError& err) {
        if (err.code() == vk::Result::eErrorOutOfPoolMemory || err.code() == vk::Result::eErrorFragmentedPool) {
            // Pool is full. Move to used list and grab a fresh one.
            mImpl->mUsedPools.push_back(mImpl->mCurrentPool);
            mImpl->mCurrentPool = mImpl->createPool();

            allocInfo.descriptorPool = mImpl->mCurrentPool;
            return DescriptorSet(mImpl->mDevice.allocateDescriptorSets(allocInfo)[0]);
        }
        throw;
    }
}

void DescriptorAllocator::reset() const {
    assert(mImpl);

    // Destroy current pool if it exists
    if (mImpl->mCurrentPool) {
        mImpl->mDevice.destroyDescriptorPool(mImpl->mCurrentPool);
        mImpl->mCurrentPool = vk::DescriptorPool{}; // Clear it
    }

    // Destroy all used pools
    for (auto pool : mImpl->mUsedPools) {
        mImpl->mDevice.destroyDescriptorPool(pool);
    }
    mImpl->mUsedPools.clear();
}

UniqueDescriptorAllocator::UniqueDescriptorAllocator(vk::Device device) {
    mImpl = new DescriptorAllocatorImpl(device);
}

UniqueDescriptorAllocator::~UniqueDescriptorAllocator() {
    if (mImpl) {
        if (mImpl->mCurrentPool) mImpl->mDevice.destroyDescriptorPool(mImpl->mCurrentPool);
        for (auto p : mImpl->mUsedPools) mImpl->mDevice.destroyDescriptorPool(p);
        delete mImpl;
        mImpl = nullptr;
    }
}

UniqueDescriptorAllocator::UniqueDescriptorAllocator(UniqueDescriptorAllocator&& other) noexcept {
    mImpl = std::exchange(other.mImpl, nullptr);
}

UniqueDescriptorAllocator& UniqueDescriptorAllocator::operator=(UniqueDescriptorAllocator&& other) noexcept {
    if (this != &other) {
        this->~UniqueDescriptorAllocator();
        mImpl = std::exchange(other.mImpl, nullptr);
    }
    return *this;
}