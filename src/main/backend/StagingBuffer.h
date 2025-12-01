#pragma once

#include <ranges>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

/// <summary>
/// A buffer to upload data from the CPU to the GPU.
/// </summary>
class StagingBuffer {
public:
    /// <summary>
    /// Creates a new staging buffer.
    /// </summary>
    /// <param name="allocator">The VMA allocator to use for buffer allocations.</param>
    /// <param name="device">The Vulkan device.</param>
    /// <param name="cmd_pool">The command pool to allocate command buffers from. It should be for a transfer queue.</param>
    StagingBuffer(const vma::Allocator &allocator, const vk::Device &device, const vk::CommandPool &cmd_pool);

    StagingBuffer() = default;

    /// <summary>
    /// Destroys the staging buffer and frees all associated resources.
    /// </summary>
    ~StagingBuffer();

    StagingBuffer(const StagingBuffer &other) = delete;
    StagingBuffer &operator=(const StagingBuffer &other) = delete;

    StagingBuffer(StagingBuffer &&other) noexcept;
    StagingBuffer &operator=(StagingBuffer &&other) noexcept;

    /// <summary>
    /// Creates a new buffer on the GPU and uploads a contiguous range of data to it.
    /// </summary>
    /// <typeparam name="R">The type of the range.</typeparam>
    /// <param name="data">The range of data to upload.</param>
    /// <param name="usage">The usage flags for the new buffer.</param>
    /// <returns>A pair containing the unique buffer and its allocation.</returns>
    template<std::ranges::contiguous_range R>
    std::pair<vma::UniqueBuffer, vma::UniqueAllocation> upload(R &&data, vk::BufferUsageFlags usage) {
        using T = std::ranges::range_value_t<R>;
        size_t size = data.size() * sizeof(T);
        return std::move(upload(data.data(), size, usage));
    }

    /// <summary>
    /// Creates a new buffer on the GPU and uploads a block of data to it.
    /// </summary>
    /// <param name="data">A pointer to the data to upload.</param>
    /// <param name="size">The size of the data in bytes.</param>
    /// <param name="usage">The usage flags for the new buffer.</param>
    /// <returns>A pair containing the unique buffer and its allocation.</returns>
    std::pair<vma::UniqueBuffer, vma::UniqueAllocation> upload(const void *data, size_t size, vk::BufferUsageFlags usage);

    /// <summary>
    /// Stages a block of data for uploading to a destination buffer on the GPU.
    /// </summary>
    /// <param name="data">A pointer to the data to upload.</param>
    /// <param name="size">The size of the data in bytes.</param>
    /// <param name="dst">The destination buffer on the GPU. It must support TRANSFER_DST usage.</param>razor
    void upload(const void *data, size_t size, const vk::Buffer &dst);

    /// <summary>
    /// Stages a contiguous range of data for uploading to a new buffer on the GPU.
    /// The buffer is created with TRANSFER_SRC usage.
    /// </summary>
    /// <typeparam name="R">The type of the range.</typeparam>
    /// <param name="data">The range of data to upload.</param>
    /// <returns>The host-visible staging buffer.</returns>
    template<std::ranges::contiguous_range R>
    vk::Buffer stage(R &&data) {
        using T = std::ranges::range_value_t<R>;
        size_t size = data.size() * sizeof(T);
        return std::move(stage(data.data(), size));
    }

    /// <summary>
    /// Stages a block of data for uploading to a new buffer on the GPU.
    /// The buffer is created with TRANSFER_SRC usage.
    /// </summary>
    /// <param name="data">A pointer to the data to upload.</param>
    /// <param name="size">The size of the data in bytes.</param>
    /// <returns>The host-visible staging buffer.</returns>
    vk::Buffer stage(const void *data, size_t size);

    /// <summary>
    /// Allocates a new staging buffer of the given size and returns it along with the mapped pointer.
    /// Use this to write data directly to the staging buffer without an intermediate copy.
    /// </summary>
    /// <param name="size">The size to allocate in bytes.</param>
    /// <returns>A pair containing the VkBuffer handle and the raw mapped pointer.</returns>
    std::pair<vk::Buffer, void *> stage(size_t size);

    /// <summary>
    /// Stages a contiguous range of data for uploading to a destination buffer on the GPU.
    /// </summary>
    /// <typeparam name="R">The type of the range.</typeparam>
    /// <param name="data">The range of data to upload.</param>
    /// <param name="dst">The destination buffer on the GPU. It must support TRANSFER_DST usage.</param>
    template<std::ranges::contiguous_range R>
    void upload(R &&data, const vk::Buffer &dst) {
        using T = std::ranges::range_value_t<R>;
        size_t size = data.size() * sizeof(T);

        upload(data.data(), size, dst);
    }

    /// <summary>
    /// Submits all staged uploads to the GPU and waits for completion.
    /// </summary>
    /// <param name="queue">The Vulkan queue to submit to. It should be a transfer queue.</param>
    void submit(const vk::Queue &queue, const vk::SubmitInfo & = {});

    /// <summary>
    /// Submits all staged uploads to the GPU. After `submitUnsynchronized`, the staging buffer must not be used while resources are still in use.
    /// </summary>
    /// <param name="queue">The Vulkan queue to submit to. It should be a transfer queue.</param>
    void submitUnsynchronized(const vk::Queue &queue, const vk::SubmitInfo & = {});

    /// <summary>
    /// After calling `submitUnsynchronized` this makes the staging buffer ready for further action. This
    /// function must only be called once the submitted resources are not in use by the GPU anymore.
    /// </summary>
    void beginUnsynchronized();

    /// <summary>
    /// Returns the command buffer used for staging operations. Only transfer commands may be permitted.
    /// </summary>
    [[nodiscard]] vk::CommandBuffer commands() const { return mCommands; }

    [[nodiscard]] vma::Allocator allocator() const { return mAllocator; }

private:
    void createCommandBuffer();

    vk::Device mDevice = {};
    vma::Allocator mAllocator = {};
    vk::CommandPool mCommandPool = {};
    vk::CommandBuffer mCommands = {};
    std::vector<std::pair<vk::Buffer, vma::Allocation>> mAllocations;
};
