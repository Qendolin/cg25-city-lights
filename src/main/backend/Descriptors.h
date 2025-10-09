#pragma once


#pragma once

#include <vulkan/vulkan.hpp>
#include <array>
#include <span>
#include <vector>

// Type-erased binding for runtime usage
struct Binding {
    uint32_t binding;
    vk::DescriptorType type;
    uint32_t count;
    vk::ShaderStageFlags stages;
    vk::DescriptorBindingFlags flags;

    constexpr Binding(uint32_t binding,
                     vk::DescriptorType type,
                     vk::ShaderStageFlags stages,
                     uint32_t count = 1,
                     vk::DescriptorBindingFlags flags = {})
        : binding(binding), type(type), count(count), stages(stages), flags(flags) {}

    constexpr operator vk::DescriptorSetLayoutBinding() const {
        return {binding, type, count, stages};
    }
};

// Typed binding for compile-time type safety
template<vk::DescriptorType Type>
struct TypedBinding {
    uint32_t binding;
    uint32_t count;
    vk::ShaderStageFlags stages;
    vk::DescriptorBindingFlags flags;

    constexpr TypedBinding(uint32_t binding,
                          vk::ShaderStageFlags stages,
                          uint32_t count = 1,
                          vk::DescriptorBindingFlags flags = {})
        : binding(binding), count(count), stages(stages), flags(flags) {}

    constexpr operator vk::DescriptorSetLayoutBinding() const {
        return {binding, Type, count, stages};
    }

    constexpr operator Binding() const {
        return {binding, Type, stages, count, flags};
    }

    static constexpr vk::DescriptorType descriptorType() { return Type; }
};

// Convenient type aliases
using CombinedImageSamplerBinding = TypedBinding<vk::DescriptorType::eCombinedImageSampler>;
using UniformBufferBinding = TypedBinding<vk::DescriptorType::eUniformBuffer>;
using StorageBufferBinding = TypedBinding<vk::DescriptorType::eStorageBuffer>;
using StorageImageBinding = TypedBinding<vk::DescriptorType::eStorageImage>;
using InlineUniformBlockBinding = TypedBinding<vk::DescriptorType::eInlineUniformBlock>;

// Base class for descriptor set layouts
class DescriptorSetLayout {
public:
    DescriptorSetLayout() = default;
    virtual ~DescriptorSetLayout() = default;

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
        : mHandle(std::move(other.mHandle)) {}

    DescriptorSetLayout& operator=(DescriptorSetLayout&& other) noexcept {
        if (this != &other) {
            mHandle = std::move(other.mHandle);
        }
        return *this;
    }

    operator vk::DescriptorSetLayout() const {
        return *mHandle;
    }

protected:
    template<typename... Bindings>
    void create(const vk::Device& device,
                vk::DescriptorSetLayoutCreateFlags flags,
                const Bindings&... bindings) {
        std::array<vk::DescriptorSetLayoutBinding, sizeof...(Bindings)> layout_bindings = {
            static_cast<vk::DescriptorSetLayoutBinding>(bindings)...
        };
        std::array<vk::DescriptorBindingFlags, sizeof...(Bindings)> binding_flags = {
            bindings.flags...
        };

        auto chain = vk::StructureChain{
            vk::DescriptorSetLayoutCreateInfo{}
                .setFlags(flags)
                .setBindings(layout_bindings),
            vk::DescriptorSetLayoutBindingFlagsCreateInfo{}
                .setBindingFlags(binding_flags)
        };

        mHandle = device.createDescriptorSetLayoutUnique(chain.get());
    }

    void create(const vk::Device& device,
                vk::DescriptorSetLayoutCreateFlags flags,
                std::span<const Binding> bindings);

private:
    vk::UniqueDescriptorSetLayout mHandle;
};

// Descriptor set wrapper with typed write helpers
class DescriptorSet {
public:

    DescriptorSet() = default;
    explicit DescriptorSet(vk::DescriptorSet set) : mHandle(set) {}

    // Generic write method
    template<vk::DescriptorType Type>
    vk::WriteDescriptorSet write(const TypedBinding<Type>& binding, uint32_t arrayElement = 0) const {
        return {
            .dstSet = mHandle,
            .dstBinding = binding.binding,
            .dstArrayElement = arrayElement,
            .descriptorCount = binding.count,
            .descriptorType = Type,
        };
    }

    // Two write overloads for the two info types
    template<vk::DescriptorType Type>
    vk::WriteDescriptorSet write(const TypedBinding<Type>& binding,
                                const vk::DescriptorImageInfo& info,
                                uint32_t arrayElement = 0) const {
        return write(binding, arrayElement).setImageInfo(info);
    }

    template<vk::DescriptorType Type>
    vk::WriteDescriptorSet write(const TypedBinding<Type>& binding,
                                const vk::DescriptorBufferInfo& info,
                                uint32_t arrayElement = 0) const {
        return write(binding, arrayElement).setBufferInfo(info);
    }

    // Special case for inline uniform blocks
    vk::WriteDescriptorSet write(const InlineUniformBlockBinding& binding,
                                const vk::WriteDescriptorSetInlineUniformBlock& block,
                                uint32_t arrayElement = 0) const {
        return write(binding, arrayElement).setPNext(&block);
    }

    operator vk::DescriptorSet() const {
        return mHandle;
    }

private:
    vk::DescriptorSet mHandle;
};

// Simple descriptor allocator
class DescriptorAllocator {
public:
    DescriptorAllocator() = default;
    explicit DescriptorAllocator(const vk::Device& device);

    DescriptorSet allocate(const DescriptorSetLayout& layout);

    void reset() {
        mDevice.resetDescriptorPool(*mPool);
    }

private:
    vk::UniqueDescriptorPool mPool;
    vk::Device mDevice = {};
};
