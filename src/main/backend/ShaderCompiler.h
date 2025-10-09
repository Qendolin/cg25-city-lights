#pragma once
#include <filesystem>
#include <memory>
#include <vector>

#include "Pipeline.h"

namespace vk {
    enum class ShaderStageFlagBits : uint32_t;
}

namespace shaderc {
    class Compiler;
}

struct ShaderCompileOptions {
    bool optimize = false;
    bool debug = false;
    bool print = false;
    std::vector<std::string> macros = {};
};

class ShaderCompiler {
public:
    ShaderCompiler();

    ~ShaderCompiler();

    [[nodiscard]] std::vector<uint32_t> compile(
            const std::filesystem::path &source_path, vk::ShaderStageFlagBits stage, ShaderCompileOptions opt
    ) const;

private:
    std::unique_ptr<shaderc::Compiler> mCompiler;

};

class ShaderLoader {
public:
    bool optimize = false;
    bool debug = false;
    bool print = false;
    std::filesystem::path root = "";

    ShaderLoader() { mCompiler = std::make_unique<ShaderCompiler>(); }

    [[nodiscard]] UniqueCompiledShaderStage loadFromSource(const vk::Device& device, const std::filesystem::path &path, std::span<std::string> macros = {}) const;
    [[nodiscard]] UniqueCompiledShaderStage loadFromBinary(const vk::Device& device, const std::filesystem::path &path, vk::ShaderStageFlagBits stage) const;

private:
    std::unique_ptr<ShaderCompiler> mCompiler;
};