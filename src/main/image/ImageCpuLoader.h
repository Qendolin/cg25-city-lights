#pragma once
#include <vector>
#include <memory>
#include "ImageTypes.h"
#include "LoadTask.h"

class ThreadPool;

class ImageCpuLoader {
public:
    using Task = LoadTask<ImageData>;
    using MultiTask = LoadTask<std::vector<ImageData>>;

    explicit ImageCpuLoader(size_t threadCount = std::thread::hardware_concurrency());
    ~ImageCpuLoader();

    Task loadAsync(const ImageSource& source);
    MultiTask loadAsync(std::span<const ImageSource> sources);

private:
    std::unique_ptr<ThreadPool> mPool;

    static std::pair<std::vector<ImageSource>, std::unique_ptr<std::byte[]>> copyAll(std::span<const ImageSource> sources);
    static ImageData loadSync(const ImageSource &source);
    static ImageData decode(const std::byte *data, size_t len);
};