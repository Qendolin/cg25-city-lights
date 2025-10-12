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

/// <summary>
/// Options for compiling a shader.
/// </summary>
struct ShaderCompileOptions {
    /// <summary>
    /// Whether to optimize the shader.
    /// </summary>
    bool optimize = false;
    /// <summary>
    /// Whether to generate debug information.
    /// </summary>
    bool debug = false;
    /// <summary>
    /// Whether to print the pre-processed result to the console.
    /// </summary>
    bool print = false;
    /// <summary>
    /// A list of macros to define.
    /// </summary>
    std::vector<std::string> macros = {};
};

/// <summary>
/// Compiles GLSL shaders to SPIR-V.
/// </summary>
class ShaderCompiler {
public:
    /// <summary>
    /// Creates a new ShaderCompiler.
    /// </summary>
    ShaderCompiler();

    ~ShaderCompiler();

    /// <summary>
    /// Compiles a shader from a source file.
    /// </summary>
    /// <param name="source_path">The path to the shader source file.</param>
    /// <param name="stage">The shader stage.</param>
    /// <param name="opt">The compilation options.</param>
    /// <returns>The compiled SPIR-V bytecode.</returns>
    [[nodiscard]] std::vector<uint32_t> compile(
            const std::filesystem::path &source_path, vk::ShaderStageFlagBits stage, ShaderCompileOptions opt
    ) const;

private:
    std::unique_ptr<shaderc::Compiler> mCompiler;

};

/// <summary>
/// Loads shaders from source or binary files.
/// </summary>
class ShaderLoader {
public:
    /// <summary>
    /// Whether to optimize the shader.
    /// </summary>
    bool optimize = false;
    /// <summary>
    /// Whether to generate debug information.
    /// </summary>
    bool debug = false;
    /// <summary>
    /// Whether to print the pre-processed result to the console.
    /// </summary>
    bool print = false;
    /// <summary>
    /// The root directory for shader files.
    /// </summary>
    std::filesystem::path root = "";

    /// <summary>
    /// Creates a new ShaderLoader.
    /// </summary>
    ShaderLoader() { mCompiler = std::make_unique<ShaderCompiler>(); }

    /// <summary>
    /// Loads a shader from a source file and compiles it.
    /// </summary>
    /// <param name="device">The Vulkan device.</param>
    /// <param name="path">The path to the shader source file.</param>
    /// <param name="macros">A list of macros to define.</param>
    /// <returns>A unique pointer to the compiled shader stage.</returns>
    [[nodiscard]] UniqueCompiledShaderStage loadFromSource(const vk::Device& device, const std::filesystem::path &path, std::span<std::string> macros = {}) const;
    
    /// <summary>
    /// Loads a shader from a binary file.
    /// </summary>
    /// <param name="device">The Vulkan device.</param>
    /// <param name="path">The path to the shader binary file.</param>
    /// <param name="stage">The shader stage.</param>
    /// <returns>A unique pointer to the compiled shader stage.</returns>
    [[nodiscard]] UniqueCompiledShaderStage loadFromBinary(const vk::Device& device, const std::filesystem::path &path, vk::ShaderStageFlagBits stage) const;

private:
    std::unique_ptr<ShaderCompiler> mCompiler;
};
