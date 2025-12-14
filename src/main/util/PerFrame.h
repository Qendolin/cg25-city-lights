#pragma once

#include <vector>
#include <type_traits>

namespace util {
    /// <summary>
    /// Manages a pool of objects, designed for resources that are cycled through on a per-frame basis (e.g., for double or triple buffering).
    /// </summary>
    /// <typeparam name="T">The type of object to manage.</typeparam>
    template<typename T>
    class PerFrame {
    public:
        PerFrame() = default;
        ~PerFrame() = default;

        PerFrame(const PerFrame &other) = delete;
        PerFrame(PerFrame &&other) noexcept = default;
        PerFrame &operator=(const PerFrame &other) = delete;
        PerFrame &operator=(PerFrame &&other) noexcept = default;

        // clang-format off
        /// <summary>
        /// Creates and initializes the pool of objects.
        /// </summary>
        /// <typeparam name="Supplier">The type of the factory function.</typeparam>
        /// <param name="frames">The number of objects to create in the pool (e.g., 2 for double buffering, 3 for triple buffering).</param>
        /// <param name="supplier">A factory function that creates objects of type T. It can optionally take an integer index.</param>
        template<typename Supplier>
        requires std::is_same_v<T, std::invoke_result_t<Supplier, int>> && std::is_invocable_v<Supplier, int> ||
                 std::is_same_v<T, std::invoke_result_t<Supplier>> && std::is_invocable_v<Supplier>
        void create(size_t frames, Supplier &&supplier) {
            mFrames = frames;
            mPool.clear();
            mPool.reserve(mFrames);
            if constexpr (std::is_invocable_v<Supplier, int>) {
                for (int i = 0; i < mFrames; ++i) {
                    mPool.emplace_back(supplier(i));
                }
            } else {
                for (int i = 0; i < mFrames; ++i) {
                    mPool.emplace_back(supplier());
                }
            }
            mIndex = 0;
        }
        // clang-format on

        /// <summary>
        /// Advances the internal index to the next frame and returns the next object.
        /// </summary>
        /// <returns>A reference to the next object.</returns>
        T &next() {
            mIndex = (mIndex + 1) % mFrames;
            return mPool.at(mIndex);
        }

        /// <summary>
        /// Peeks at the object for the next frame without advancing the index.
        /// </summary>
        /// <returns>A const reference to the next object in the pool.</returns>
        [[nodiscard]] T &peek() const { return mPool.at((mIndex + 1) % mFrames); }

        /// <summary>
        /// Gets the object for the current frame.
        /// </summary>
        /// <returns>A const reference to the current object.</returns>
        [[nodiscard]] T &get() const { return mPool.at(mIndex); }

        /// <summary>
        /// Gets the object at a specific index in the pool.
        /// </summary>
        /// <param name="index">The index of the object to retrieve.</param>
        /// <returns>A const reference to the object at the specified index.</returns>
        [[nodiscard]] T &get(size_t index) const { return mPool.at(index); }

        [[nodiscard]] size_t size() const { return mFrames; }

        [[nodiscard]] int32_t index() const { return mIndex; }

        [[nodiscard]] bool initialized() const { return mIndex >= 0; }

    private:
        /// <summary>
        /// The pool of objects.
        /// </summary>
        mutable std::vector<T> mPool;
        /// <summary>
        /// The index of the current frame.
        /// </summary>
        int32_t mIndex = -1;
        /// <summary>
        /// The total number of frames/objects in the pool.
        /// </summary>
        size_t mFrames = 3;
    };
} // namespace util
