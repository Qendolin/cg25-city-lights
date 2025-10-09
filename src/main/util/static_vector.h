#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <stdexcept>

namespace util {
    /// <summary>static_</summary>
    template<typename T, std::size_t N>
    class static_vector {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using iterator = typename std::array<T, N>::iterator;
        using const_iterator = typename std::array<T, N>::const_iterator;

        constexpr static_vector(){}; // NOLINT(*-use-equals-default)

        constexpr static_vector(std::initializer_list<T> init) {
            if (init.size() > N)
                throw std::out_of_range("Initializer list too large");
            std::copy(init.begin(), init.end(), mStorage.begin());
            mLength = init.size();
        }

        template<typename InputIt>
        constexpr static_vector(InputIt first, InputIt last) {
            size_t count = std::distance(first, last);
            if (count > N)
                throw std::out_of_range("Range exceeds static_vector capacity");
            std::copy(first, last, mStorage.begin());
            mLength = count;
        }

        template<std::size_t M>
        constexpr static_vector(const std::array<T, M> &arr) // NOLINT(*-explicit-constructor)
        {
            static_assert(M <= N, "Array size exceeds static_vector capacity");
            std::copy(arr.begin(), arr.end(), mStorage.begin());
            mLength = M;
        }

        [[nodiscard]] constexpr bool empty() const noexcept { return mLength == 0; }
        [[nodiscard]] constexpr bool full() const noexcept { return mLength == N; }
        [[nodiscard]] constexpr size_t size() const noexcept { return mLength; }
        // ReSharper disable once CppMemberFunctionMayBeStatic
        [[nodiscard]] constexpr size_t capacity() const noexcept { return N; }
        [[nodiscard]] constexpr T *data() noexcept { return mStorage.data(); }
        [[nodiscard]] constexpr const T *data() const noexcept { return mStorage.data(); }

        constexpr void clear() noexcept { mLength = 0; }

        constexpr void push_back(const T &value) {
            if (mLength >= N)
                throw std::out_of_range("static_vector capacity exceeded");
            mStorage[mLength++] = value;
        }

        constexpr void push_back(T &&value) {
            if (mLength >= N)
                throw std::out_of_range("static_vector capacity exceeded");
            mStorage[mLength++] = std::move(value);
        }

        template<typename... Args>
        constexpr T &emplace_back(Args &&...args) {
            if (mLength >= N)
                throw std::out_of_range("static_vector capacity exceeded");
            return mStorage[mLength++] = T(std::forward<Args>(args)...);
        }

        void resize(size_t size) {
            if (size > N)
                throw std::out_of_range("static_vector capacity exceeded");
            if (size > mLength)
                mLength = size;
        }

        constexpr void pop_back() {
            assert(mLength > 0 && "static_vector is empty");
            --mLength;
        }

        constexpr T &front() {
            assert(mLength > 0 && "static_vector is empty");
            return mStorage[0];
        }

        constexpr const T &front() const {
            assert(mLength > 0 && "static_vector is empty");
            return mStorage[0];
        }

        constexpr T &back() {
            assert(mLength > 0 && "static_vector is empty");
            return mStorage[mLength - 1];
        }

        constexpr const T &back() const {
            assert(mLength > 0 && "static_vector is empty");
            return mStorage[mLength - 1];
        }

        constexpr T &operator[](size_t index) { return mStorage[index]; }
        constexpr const T &operator[](size_t index) const { return mStorage[index]; }

        constexpr T &at(size_t index) {
            if (index >= mLength)
                throw std::out_of_range("Index out of range");
            return mStorage[index];
        }

        constexpr const T &at(size_t index) const {
            if (index >= mLength)
                throw std::out_of_range("Index out of range");
            return mStorage[index];
        }

        constexpr iterator begin() noexcept { return mStorage.begin(); }
        constexpr const_iterator begin() const noexcept { return mStorage.begin(); }
        constexpr const_iterator cbegin() const noexcept { return mStorage.cbegin(); }

        constexpr iterator end() noexcept { return mStorage.begin() + mLength; }
        constexpr const_iterator end() const noexcept { return mStorage.begin() + mLength; }
        constexpr const_iterator cend() const noexcept { return mStorage.cbegin() + mLength; }

        constexpr void erase(iterator pos) {
            if (pos < begin() || pos >= end())
                throw std::out_of_range("Iterator out of range");
            std::move(pos + 1, end(), pos);
            --mLength;
        }

        constexpr void erase(iterator first, iterator last) {
            if (first < begin() || last > end() || first > last)
                throw std::out_of_range("Iterator range invalid");
            std::move(last, end(), first);
            mLength -= last - first;
        }

    private:
        std::array<T, N> mStorage = {};
        std::size_t mLength = 0;
    };
} // namespace util
