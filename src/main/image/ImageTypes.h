#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <filesystem>
#include <format>
#include <span>
#include <stdexcept>
#include <variant>
#include <vulkan/vulkan.hpp>

// -----------------------------------------------------------------------------
// Enums & Helpers
// -----------------------------------------------------------------------------

enum class ComponentSwizzle {
    Identity = VK_COMPONENT_SWIZZLE_IDENTITY,
    Zero = VK_COMPONENT_SWIZZLE_ZERO,
    One = VK_COMPONENT_SWIZZLE_ONE,
    R = VK_COMPONENT_SWIZZLE_R,
    G = VK_COMPONENT_SWIZZLE_G,
    B = VK_COMPONENT_SWIZZLE_B,
    A = VK_COMPONENT_SWIZZLE_A
};

struct ComponentMapping {
    ComponentSwizzle r = ComponentSwizzle::R;
    ComponentSwizzle g = ComponentSwizzle::G;
    ComponentSwizzle b = ComponentSwizzle::B;
    ComponentSwizzle a = ComponentSwizzle::A;

    ComponentSwizzle operator[](int i) const {
        switch (i) {
            case 0: return r;
            case 1: return g;
            case 2: return b;
            case 3: return a;
            default: throw std::out_of_range("Component index out of bounds.");
        }
    }

    [[nodiscard]] bool isDefault() const {
        return r == ComponentSwizzle::R && g == ComponentSwizzle::G &&
               b == ComponentSwizzle::B && a == ComponentSwizzle::A;
    }
};

struct ComponentType {
    uint32_t size = 0;
    bool isInteger = false;
    bool isSigned = false;
    bool isPacked = false;
    std::array<std::byte, 4> one = {};

    static const ComponentType None;
    static const ComponentType UInt8;
    static const ComponentType UInt16;
    static const ComponentType UInt32;
    static const ComponentType Float;
    static const ComponentType PackedRGB9E5;

private:
    template<typename T>
    static consteval std::array<std::byte, 4> one_to_bytes(T value) {
        static_assert(sizeof(T) <= 4, "Type too large for 4-byte buffer");

        std::array<std::byte, 4> result = {};

        // std::bit_cast creates a new object of the destination type
        // by copying the bits of the source. It is constexpr-safe.
        auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);

        for (size_t i = 0; i < sizeof(T); ++i) {
            result[i] = bytes[i];
        }

        return result;
    }
};

// -----------------------------------------------------------------------------
// Data Containers
// -----------------------------------------------------------------------------

// Typedef for the deleter function
using ImageDeleter = void(*)(void*);

// Default deleter that calls std::free (matches stbi and malloc)
inline void MallocFreeDeleter(void* ptr) { std::free(ptr); }

struct ImageData {
    std::unique_ptr<std::byte[], ImageDeleter> data = {nullptr, MallocFreeDeleter};

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t components = 0;
    ComponentType componentType = ComponentType::None;

    static ImageData create(const ImageData &reference, ComponentType type, uint32_t components);
    static ImageData create(const ImageData &reference) {
        return create(reference, reference.componentType, reference.components);
    }

    constexpr size_t index(uint32_t x, uint32_t y, uint32_t component) const {
        return ((y * width + x) * components + component) * componentType.size;
    }

    template<typename T>
    constexpr const T &value(uint32_t x, uint32_t y, uint32_t component) const {
        return *reinterpret_cast<T *>(data.get() + index(x, y, component));
    }

    template<typename T>
    constexpr T &value(uint32_t x, uint32_t y, uint32_t component) {
        return *reinterpret_cast<T *>(data.get() + index(x, y, component));
    }

    [[nodiscard]] size_t size() const { return width * height * components * componentType.size; }

    static void copy(const ImageData &src, ImageData &dst, ComponentMapping component_map = {});
    static void copy(const ImageData &src, ImageData &dst, uint32_t src_component, uint32_t dst_component);
    static void fill(ImageData &dst, uint32_t dst_component, const std::byte *value);
};

/// <summary>
/// Basic metadata for an image source (file or memory blob).
/// </summary>
struct ImageSourceInfo {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    ComponentType componentType = ComponentType::None;
};

/// <summary>
/// A stateful handle to an image source (Path or Memory).
/// Automatically parses metadata upon construction.
/// </summary>
struct ImageSource {
    explicit ImageSource(std::filesystem::path path);
    explicit ImageSource(std::span<const std::byte> data, const std::string& name = "");

    std::variant<std::filesystem::path, std::span<const std::byte>> variant;
    ImageSourceInfo info;
    std::string name;
};