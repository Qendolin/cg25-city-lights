#include "ImageTypes.h"

#include <filesystem>
#include <stb_image.h>
#include <format>


// Definition of static members
const ComponentType ComponentType::None = {};
const ComponentType ComponentType::UInt8 = {sizeof(uint8_t), true, false, false, ComponentType::one_to_bytes<uint8_t>(1)};
const ComponentType ComponentType::UInt16 = {sizeof(uint16_t), true, false, false, ComponentType::one_to_bytes<uint16_t>(1)};
const ComponentType ComponentType::UInt32 = {sizeof(uint32_t), true, false, false, ComponentType::one_to_bytes<uint32_t>(1)};
const ComponentType ComponentType::Float = {sizeof(float), false, true, false, ComponentType::one_to_bytes<float>(1.0f)};
const ComponentType ComponentType::PackedRGB9E5 = {sizeof(uint32_t), false, false, true, {}};

void ImageData::copy(const ImageData &src, ImageData &dst, ComponentMapping component_map) {
    std::array<std::byte, 4> zero = {};
    for (uint32_t c = 0; c < dst.components; c++) {
        switch (component_map[c]) {
            case ComponentSwizzle::Identity:
                // Assumes identity means 0->0, 1->1. If src/dst differ in count, handle carefully.
                if (c < src.components)
                    copy(src, dst, c, c);
                break;
            case ComponentSwizzle::R:
                copy(src, dst, 0, c);
                break;
            case ComponentSwizzle::G:
                copy(src, dst, 1, c);
                break;
            case ComponentSwizzle::B:
                copy(src, dst, 2, c);
                break;
            case ComponentSwizzle::A:
                copy(src, dst, 3, c);
                break;
            case ComponentSwizzle::Zero:
                fill(dst, c, zero.data());
                break;
            case ComponentSwizzle::One:
                if (dst.componentType.isPacked)
                    throw std::runtime_error("Cannot set packed components to one.");
                fill(dst, c, dst.componentType.one.data());
                break;
        }
    }
}

void ImageData::copy(const ImageData &src, ImageData &dst, uint32_t src_component, uint32_t dst_component) {
    uint32_t w = std::min(src.width, dst.width);
    uint32_t h = std::min(src.height, dst.height);

    if (src.componentType.size != dst.componentType.size)
        throw std::runtime_error("Cannot copy between image data with different component sizes.");

    if (src_component >= src.components || dst_component >= dst.components)
        throw std::runtime_error("Component index out of bounds.");

    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            size_t src_i = src.index(x, y, src_component);
            size_t dst_i = dst.index(x, y, dst_component);
            std::memcpy(dst.data.get() + dst_i, src.data.get() + src_i, src.componentType.size);
        }
    }
}

void ImageData::fill(ImageData &dst, uint32_t dst_component, const std::byte *value) {
    if (dst_component >= dst.components)
        throw std::runtime_error("Component index out of bounds.");
    for (uint32_t y = 0; y < dst.height; y++) {
        for (uint32_t x = 0; x < dst.width; x++) {
            size_t dst_i = dst.index(x, y, dst_component);
            std::memcpy(dst.data.get() + dst_i, value, dst.componentType.size);
        }
    }
}


ImageData ImageData::create(const ImageData &reference, ComponentType type, uint32_t components) {
    size_t totalSize = reference.width * reference.height * components * type.size;

    // Allocate uninitialized memory
    // We use malloc to be consistent with the default MallocFreeDeleter
    std::byte *ptr = static_cast<std::byte *>(std::malloc(totalSize));

    if (!ptr && totalSize > 0) {
        throw std::bad_alloc();
    }

    ImageData result;
    result.width = reference.width;
    result.height = reference.height;
    result.components = components;
    result.componentType = type;

    // Wrap in unique_ptr with the standard free deleter
    result.data = std::unique_ptr<std::byte[], ImageDeleter>(ptr, MallocFreeDeleter);

    return result;
}

inline ComponentType getTypeFromStbi(bool is16, bool isHdr) {
    if (isHdr)
        return ComponentType::Float;
    if (is16)
        return ComponentType::UInt16;
    return ComponentType::UInt8;
}

ImageSource::ImageSource(std::filesystem::path path) : variant(path), name(path.filename().string()) {
    int w, h, comp;
    std::string p = path.string();
    if (stbi_info(p.c_str(), &w, &h, &comp) == 0) {
        throw std::runtime_error(
                std::format("Failed to read image header of '{}': {}", path.string(), std::string(stbi_failure_reason()))
        );
    }

    bool is16 = stbi_is_16_bit(p.c_str());
    bool isHdr = stbi_is_hdr(p.c_str());

    info = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint32_t>(comp), getTypeFromStbi(is16, isHdr)};
}

ImageSource::ImageSource(std::span<const std::byte> data, const std::string& name) : variant(data), name(name) {
    int w, h, comp;
    auto *buffer = reinterpret_cast<const stbi_uc *>(data.data());

    if (stbi_info_from_memory(buffer, data.size(), &w, &h, &comp) == 0) {
        throw std::runtime_error("Failed to read unnamed image header: " + std::string(stbi_failure_reason()));
    }

    bool is16 = stbi_is_16_bit_from_memory(buffer, data.size());
    bool isHdr = stbi_is_hdr_from_memory(buffer, data.size());

    info = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), static_cast<uint32_t>(comp), getTypeFromStbi(is16, isHdr)};
}
