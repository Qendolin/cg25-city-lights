#include "ImageCpuLoader.h"

#include <filesystem>
#include <fstream>
#include <queue>
#include <stb_image.h>
#include <thread>

#include "../util/Logger.h"

static std::vector<std::byte> readFile(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0)
        return {};

    std::vector<std::byte> buffer(size);
    if (!file.read(reinterpret_cast<char *>((buffer.data())), size)) {
        throw std::runtime_error("Failed to read file data: " + path.string());
    }
    return buffer;
}

// -----------------------------------------------------------------------------
// Minimal ThreadPool Implementation
// -----------------------------------------------------------------------------
class ThreadPool {
    std::vector<std::jthread> workers;
    std::queue<std::move_only_function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;

public:
    ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
                while (true) {
                    std::move_only_function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                }
            });
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();
    }

    void enqueue(std::move_only_function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }
};

// -----------------------------------------------------------------------------
// ImageCpuLoader Implementation
// -----------------------------------------------------------------------------

ImageCpuLoader::ImageCpuLoader(size_t threadCount) : mPool(std::make_unique<ThreadPool>(threadCount)) {}

ImageCpuLoader::~ImageCpuLoader() = default;

ImageData ImageCpuLoader::decode(const std::byte *buffer, size_t len_) {
    int w, h, ch;
    const auto *data = reinterpret_cast<const stbi_uc *>(buffer);

    if (len_ >= std::numeric_limits<int>::max()) {
        throw std::runtime_error("Image data too large for stb_image");
    }
    int len = static_cast<int>(len_);

    bool isHdr = stbi_is_hdr_from_memory(data, len);
    bool is16 = stbi_is_16_bit_from_memory(data, len);

    void *result = nullptr;
    ComponentType type = ComponentType::None;

    if (isHdr) {
        result = stbi_loadf_from_memory(data, len, &w, &h, &ch, 0);
        type = ComponentType::Float;
    } else if (is16) {
        result = stbi_load_16_from_memory(data, len, &w, &h, &ch, 0);
        type = ComponentType::UInt16;
    } else {
        result = stbi_load_from_memory(data, len, &w, &h, &ch, 0);
        type = ComponentType::UInt8;
    }

    if (!result)
        throw std::runtime_error("Failed to decode image: " + std::string(stbi_failure_reason()));

    ImageData out;
    out.width = static_cast<uint32_t>(w);
    out.height = static_cast<uint32_t>(h);
    out.components = static_cast<uint32_t>(ch);
    out.componentType = type;

    // We use stbi_image_free as the custom deleter.
    out.data = {static_cast<std::byte *>(result), stbi_image_free};

    return out;
}

ImageCpuLoader::Task ImageCpuLoader::loadAsync(const ImageSource &source) {
    Task task;

    auto [copies, data] = copyAll(std::array{source});
    mPool->enqueue([t = task, s = copies[0], d = std::move(data)]() mutable {
        try {
            t.resolve(loadSync(s));
        } catch (const std::exception &e) {
            Logger::error(std::format("Failed to load image {}: {}", s.name, e.what()));
            t.resolveError(std::make_exception_ptr(e));
        }
    });
    return task;
}

ImageCpuLoader::MultiTask ImageCpuLoader::loadAsync(std::span<const ImageSource> sources) {
    MultiTask task;
    auto [copies, data] = copyAll(sources);

    mPool->enqueue([t = task, s = std::move(copies), d = std::move(data)]() mutable {
        std::vector<ImageData> results;
        if (s.empty()) {
            t.resolve(std::move(results));
            return;
        }
        const auto *latest = &s.front();
        try {
            results.reserve(s.size());
            for (const auto &source: s) {
                latest = &source;
                results.emplace_back(loadSync(source));
            }
            t.resolve(std::move(results));
        } catch (const std::exception &e) {
            Logger::error(std::format("Failed to load image '{}': {}", latest->name, e.what()));
            t.resolveError(std::make_exception_ptr(e));
        }
    });
    return task;
}

std::pair<std::vector<ImageSource>, std::unique_ptr<std::byte[]>> ImageCpuLoader::copyAll(std::span<const ImageSource> sources
) {
    size_t total_size = 0;
    for (const auto &source: sources) {
        if (std::holds_alternative<std::span<const std::byte>>(source.variant)) {
            total_size += std::get<std::span<const std::byte>>(source.variant).size();
        }
    }

    // ensure stable storage for copied data
    std::unique_ptr<std::byte[]> combined_buffer = nullptr;
    std::byte* write_ptr = nullptr;

    if (total_size > 0) {
        combined_buffer = std::make_unique<std::byte[]>(total_size);
        write_ptr = combined_buffer.get();
    }

    std::vector<ImageSource> copies;
    copies.reserve(sources.size());
    for (const auto &source: sources) {
        auto &source_copy = copies.emplace_back(source); // copy over

        if (std::holds_alternative<std::span<const std::byte>>(source.variant)) {
            auto src_buffer = std::get<std::span<const std::byte>>(source.variant);
            std::memcpy(write_ptr, src_buffer.data(), src_buffer.size());
            source_copy.variant = std::move(std::span(write_ptr, src_buffer.size()));
            write_ptr += src_buffer.size();
        }
    }
    return std::make_pair(std::move(copies), std::move(combined_buffer));
}

ImageData ImageCpuLoader::loadSync(const ImageSource &source) {
    if (std::holds_alternative<std::filesystem::path>(source.variant)) {
        std::vector<std::byte> buffer = readFile(std::get<std::filesystem::path>(source.variant));
        return decode(buffer.data(), buffer.size());
    } else {
        auto buffer = std::get<std::span<const std::byte>>(source.variant);
        return decode(buffer.data(), buffer.size());
    }
}
