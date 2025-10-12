#pragma once

#include <vulkan/vulkan.hpp>
#include <array>
#include <span>
#include <vector>

/// <summary>
/// A type-erased descriptor set binding for runtime usage.
/// </summary>
struct Binding {
    uint32_t binding;
    vk::DescriptorType type;
    uint32_t count;
    vk::ShaderStageFlags stages;
    vk::DescriptorBindingFlags flags;

    /// <summary>
    /// Constructs a new Binding.
    /// </summary>
    /// <param name="binding">The binding number in the descriptor set.</param>
    /// <param name="type">The type of the descriptor.</param>
    /// <param name="stages">The shader stages that can access this binding.</param>
    /// <param name="count">The number of descriptors in the binding (for arrays).</param>
    /// <param name="flags">Additional flags for the binding.</param>
    constexpr Binding(uint32_t binding,
                     vk::DescriptorType type,
                     vk::ShaderStageFlags stages,
                     uint32_t count = 1,
                     vk::DescriptorBindingFlags flags = {})
        : binding(binding), type(type), count(count), stages(stages), flags(flags) {}

    /// <summary>
    /// Implicit conversion to a Vulkan DescriptorSetLayoutBinding.
    /// </summary>
    constexpr operator vk::DescriptorSetLayoutBinding() const {
        return {binding, type, count, stages};
    }
};

/// <summary>
/// A typed descriptor set binding for compile-time type safety.
/// </summary>
/// <typeparam name="Type">The vk::DescriptorType of the binding.</typeparam>
template<vk::DescriptorType Type>
struct TypedBinding {
    uint32_t binding;
    uint32_t count;
    vk::ShaderStageFlags stages;
    vk::DescriptorBindingFlags flags;

    /// <summary>
    /// Constructs a new TypedBinding.
    /// </summary>
    /// <param name="binding">The binding number in the descriptor set.</param>
    /// <param name="stages">The shader stages that can access this binding.</param>
    /// <param name="count">The number of descriptors in the binding (for arrays).</param>
    /// <param name="flags">Additional flags for the binding.</param>
    constexpr TypedBinding(uint32_t binding,
                          vk::ShaderStageFlags stages,
                          uint32_t count = 1,
                          vk::DescriptorBindingFlags flags = {})
        : binding(binding), count(count), stages(stages), flags(flags) {}

    /// <summary>
    /// Implicit conversion to a Vulkan DescriptorSetLayoutBinding.
    /// </summary>
    constexpr operator vk::DescriptorSetLayoutBinding() const {
        return {binding, Type, count, stages};
    }

    /// <summary>
    /// Implicit conversion to a type-erased Binding.
    /// </summary>
    constexpr operator Binding() const {
        return {binding, Type, stages, count, flags};
    }

    /// <summary>
    /// Gets the descriptor type of this binding at compile time.
    /// </summary>
    /// <returns>The vk::DescriptorType.</returns>
    static constexpr vk::DescriptorType descriptorType() { return Type; }
};

// Convenient type aliases for common descriptor types.
using CombinedImageSamplerBinding = TypedBinding<vk::DescriptorType::eCombinedImageSampler>;
using UniformBufferBinding = TypedBinding<vk::DescriptorType::eUniformBuffer>;
using StorageBufferBinding = TypedBinding<vk::DescriptorType::eStorageBuffer>;
using StorageImageBinding = TypedBinding<vk::DescriptorType::eStorageImage>;
using InlineUniformBlockBinding = TypedBinding<vk::DescriptorType::eInlineUniformBlock>;

/// <summary>
/// Base class for managing Vulkan Descriptor Set Layouts.
/// It handles the creation and lifetime of a vk::DescriptorSetLayout.
/// </summary>
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

    /// <summary>
    /// Implicit conversion to the underlying vk::DescriptorSetLayout handle.
    /// </summary>
    operator vk::DescriptorSetLayout() const {
        return *mHandle;
    }

protected:
    /// <summary>
    /// Creates the descriptor set layout from a variadic list of bindings.
    /// </summary>
    /// <typeparam name="...Bindings">A parameter pack of TypedBinding or Binding types.</typeparam>
    /// <param name="device">The logical Vulkan device.</param>
    /// <param name="flags">Creation flags for the descriptor set layout.</param>
    /// <param name="...bindings">The descriptor set bindings.</param>
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

    /// <summary>
    /// Creates the descriptor set layout from a span of type-erased bindings.
    /// </summary>
    /// <param name="device">The logical Vulkan device.</param>
    /// <param name="flags">Creation flags for the descriptor set layout.</param>
    /// <param name="bindings">A span of descriptor set bindings.</param>
    void create(const vk::Device& device,
                vk::DescriptorSetLayoutCreateFlags flags,
                std::span<const Binding> bindings);

private:
    vk::UniqueDescriptorSetLayout mHandle;
};

/// <summary>
/// A wrapper for vk::DescriptorSet providing typed helper methods for descriptor writes.
/// </summary>
class DescriptorSet {
public:

    DescriptorSet() = default;
    explicit DescriptorSet(vk::DescriptorSet set) : mHandle(set) {}

    /// <summary>
    /// Creates a generic vk::WriteDescriptorSet structure for a given typed binding.
    /// </summary>
    /// <typeparam name="Type">The descriptor type of the binding.</typeparam>
    /// <param name="binding">The typed binding to write to.</param>
    /// <param name="arrayElement">The starting element in the descriptor array.</param>
    /// <returns>A partially filled vk::WriteDescriptorSet structure.</returns>
    template<vk::DescriptorType Type>
    [[nodiscard]] vk::WriteDescriptorSet write(const TypedBinding<Type>& binding, uint32_t arrayElement = 0) const {
        return {
            .dstSet = mHandle,
            .dstBinding = binding.binding,
            .dstArrayElement = arrayElement,
            .descriptorCount = binding.count,
            .descriptorType = Type,
        };
    }

    /// <summary>
    /// Creates a vk::WriteDescriptorSet for a binding with image information.
    /// </summary>
    /// <typeparam name="Type">The descriptor type of the binding.</typeparam>
    /// <param name="binding">The typed binding to write to.</param>
    /// <param name="info">The descriptor image info.</param>
    /// <param name="arrayElement">The starting element in the descriptor array.</param>
    /// <returns>A complete vk::WriteDescriptorSet structure for an image descriptor.</returns>
    template<vk::DescriptorType Type>
    [[nodiscard]] vk::WriteDescriptorSet write(const TypedBinding<Type>& binding,
                                const vk::DescriptorImageInfo& info,
                                uint32_t arrayElement = 0) const {
        return write(binding, arrayElement).setImageInfo(info);
    }

    /// <summary>
    /// Creates a vk::WriteDescriptorSet for a binding with buffer information.
    /// </summary>
    /// <typeparam name="Type">The descriptor type of the binding.</typeparam>
    /// <param name="binding">The typed binding to write to.</param>
    /// <param name="info">The descriptor buffer info.</param>
    /// <param name="arrayElement">The starting element in the descriptor array.</param>
    /// <returns>A complete vk::WriteDescriptorSet structure for a buffer descriptor.</returns>
    template<vk::DescriptorType Type>
    [[nodiscard]] vk::WriteDescriptorSet write(const TypedBinding<Type>& binding,
                                const vk::DescriptorBufferInfo& info,
                                uint32_t arrayElement = 0) const {
        return write(binding, arrayElement).setBufferInfo(info);
    }

    /// <summary>
    /// Creates a vk::WriteDescriptorSet for an inline uniform block.
    /// </summary>
    /// <param name="binding">The inline uniform block binding.</param>
    /// <param name="block">The inline uniform block data to write.</param>
    /// <param name="arrayElement">The starting element in the descriptor array.</param>
    /// <returns>A complete vk::WriteDescriptorSet structure for an inline uniform block.</returns>
    [[nodiscard]] vk::WriteDescriptorSet write(const InlineUniformBlockBinding& binding,
                                const vk::WriteDescriptorSetInlineUniformBlock& block,
                                uint32_t arrayElement = 0) const {
        return write(binding, arrayElement).setPNext(&block);
    }

    /// <summary>
    /// Implicit conversion to the underlying vk::DescriptorSet handle.
    /// </summary>
    operator vk::DescriptorSet() const {
        return mHandle;
    }

private:
    vk::DescriptorSet mHandle;
};

/// <summary>
/// A simple allocator for creating descriptor sets from a managed descriptor pool.
/// </summary>
class DescriptorAllocator {
public:
    DescriptorAllocator() = default;
    explicit DescriptorAllocator(const vk::Device& device);

    /// <summary>
    /// Allocates a single descriptor set from the pool.
    /// </summary>
    /// <param name="layout">The layout for the descriptor set to allocate.</param>
    /// <returns>An allocated DescriptorSet.</returns>
    DescriptorSet allocate(const DescriptorSetLayout& layout);

    /// <summary>
    /// Resets the underlying descriptor pool, invalidating all allocated sets.
    /// </summary>
    void reset() {
        mDevice.resetDescriptorPool(*mPool);
    }

private:
    vk::UniqueDescriptorPool mPool;
    vk::Device mDevice = {};
};
