#pragma once
#include "BufferResource.h"
#include "StagingBuffer.h"

struct BufferCreateInfo {
    size_t size = 0;
    vk::BufferUsageFlags usage = {};
    vma::AllocationCreateFlags flags = {};
    vma::MemoryUsage device = vma::MemoryUsage::eAuto;
    vk::MemoryPropertyFlags requiredProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    vk::MemoryPropertyFlags preferredProperties = {};
};

struct BufferBase : protected BufferResource {
    size_t size = 0;

    BufferBase() = default;
    explicit BufferBase(size_t size) : size(size) {}

    BufferBase(BufferBase &&other) noexcept = default;
    BufferBase &operator=(BufferBase &&other) noexcept = default;

    virtual operator vk::Buffer() const = 0; // NOLINT(*-explicit-constructor)
    explicit virtual operator bool() const = 0;


    /// <summary>
    /// Inserts an buffer memory barrier for this buffer.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier into.</param>
    /// <param name="begin">The resource access state before the barrier.</param>
    /// <param name="end">The resource access state after the barrier.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const BufferResourceAccess &begin, const BufferResourceAccess &end) const;

    /// <summary>
    /// Inserts an buffer memory barrier, transitioning the buffer to a single state.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier into.</param>
    /// <param name="single">The resource access state to transition to.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const BufferResourceAccess &single) const;

    /// <summary>
    /// Transfers ownership of the buffer between queue families.
    /// It does NOT perform any memory barriers or layout transitions. Execution ordering must be handled with a semaphore.
    /// </summary>
    /// <param name="src_cmd_buf">The command buffer in the source queue to record the barrier into.</param>
    /// <param name="dst_cmd_buf">The command buffer in the destination queue to record the barrier into.</param>
    /// <param name="src_queue">The index of the source queue family.</param>
    /// <param name="dst_queue">The index of the destination queue family.</param>
    void transfer(vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue) const;
};

struct UnmanagedBuffer : BufferBase {
    vk::Buffer buffer = {};

    UnmanagedBuffer() = default;
    /// <summary>
    /// Constructs a Buffer from an existing Vulkan buffer.
    /// </summary>
    /// <param name="buffer">A vulkan buffer handle.</param>
    /// <param name="size">The size, in bytes, of the buffer.</param>
    UnmanagedBuffer(vk::Buffer buffer, size_t size) : BufferBase(size), buffer(buffer) {}

    operator vk::Buffer() const override { // NOLINT(*-explicit-constructor)
        return buffer;
    }
    explicit operator bool() const override { return buffer; }
};


/// <summary>
/// Represents a GPU texture buffer, which is a wrapper around a Vulkan buffer and its memory allocation.
/// This class is a move-only type.
/// </summary>
struct Buffer : BufferBase {
    vma::UniqueBuffer buffer;
    vma::UniqueAllocation allocation;

    /// <summary>
    /// Creates an empty, invalid Buffer object.
    /// </summary>
    Buffer() = default;

    /// <summary>
    /// Constructs a Buffer from an existing Vulkan buffer and allocation.
    /// </summary>
    /// <param name="buffer">A VMA unique buffer handle.</param>
    /// <param name="allocation">A VMA unique allocation handle.</param>
    /// <param name="size">The size, in bytes, of the buffer.</param>
    Buffer(vma::UniqueBuffer &&buffer, vma::UniqueAllocation &&allocation, size_t size);

    Buffer(Buffer &&other) noexcept = default;
    Buffer &operator=(Buffer &&other) noexcept = default;

    /// <summary>
    /// Creates a new buffer.
    /// </summary>
    /// <param name="allocator">The VMA allocator.</param>
    /// <param name="create_info">The creation info for the buffer.</param>
    /// <returns>A new Buffer object.</returns>
    static Buffer create(const vma::Allocator &allocator, const BufferCreateInfo &create_info);

    operator vk::Buffer() const override { // NOLINT(*-explicit-constructor)
        return *buffer;
    }
    explicit operator bool() const override { return *buffer; }
};

struct TransientBufferAllocatorImpl;

/// <summary>
/// A lightweight handle to a linear buffer allocator.
/// Allocations are valid for the current frame only.
/// Copies are cheap and reference the same underlying memory pool.
/// </summary>
class TransientBufferAllocator {
public:
    TransientBufferAllocator() = default;

    /// <summary>
    /// Allocates a buffer.
    /// Valid for usage with offset=0 and size=VK_WHOLE_SIZE.
    /// </summary>
    [[nodiscard]] UnmanagedBuffer allocate(vk::DeviceSize size, vk::BufferUsageFlags usage) const;

    /// <summary>
    /// Invalidates all buffers allocated since the last reset.
    /// </summary>
    void reset();

    operator bool() const { return mImpl != nullptr; }

protected:
    TransientBufferAllocatorImpl *mImpl = nullptr;
    friend class UniqueTransientBufferAllocator;
};

/// <summary>
/// RAII Owner for the allocator implementation.
/// Manages the lifecycle of the underlying memory pool.
/// </summary>
class UniqueTransientBufferAllocator : public TransientBufferAllocator {
public:
    UniqueTransientBufferAllocator() = default;
    UniqueTransientBufferAllocator(
            const vk::Device &device, const vma::Allocator &allocator, vk::DeviceSize capacity = 64 * 1024 * 1024
    );
    ~UniqueTransientBufferAllocator();

    UniqueTransientBufferAllocator(UniqueTransientBufferAllocator &&other) noexcept;
    UniqueTransientBufferAllocator &operator=(UniqueTransientBufferAllocator &&other) noexcept;
};
