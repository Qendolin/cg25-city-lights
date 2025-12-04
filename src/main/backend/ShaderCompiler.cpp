#include "ShaderCompiler.h"

#include <filesystem>
#include <fstream>
#include <shaderc/shaderc.hpp>
#include <utility>
#include <vulkan/vulkan.hpp>

#include "../debug/Annotation.h"
#include "../util/Logger.h"


static std::string read_file(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Logger::fatal("Error opening file: " + std::filesystem::absolute(path).string());
    }

    file.seekg(0, std::ios::end);
    std::streampos size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content;
    content.resize(size);
    file.read(content.data(), size);
    file.close();

    return content;
}

class ShaderIncluder final : public shaderc::CompileOptions::IncluderInterface {
    struct IncludeResult : shaderc_include_result {
        const std::string source_name_str;
        const std::string content_str;

        IncludeResult(std::string source_name, std::string content)
            : shaderc_include_result(), source_name_str(std::move(source_name)), content_str(std::move(content)) {
            this->source_name = source_name_str.data();
            this->source_name_length = source_name_str.size();
            this->content = content_str.data();
            this->content_length = content_str.size();
            this->user_data = nullptr;
        }
    };

    shaderc_include_result *GetInclude(
            const char *requested_source, shaderc_include_type type, const char *requesting_source, size_t
    ) override {
        std::filesystem::path file_path;
        if (type == shaderc_include_type_relative) {
            file_path = std::filesystem::path(requesting_source).parent_path() / requested_source;
            if (!std::filesystem::exists(file_path))
                Logger::fatal(
                        "Shader file " + std::string(requested_source) + " loaded from " +
                        std::string(requesting_source) + " does not exist"
                );
        } else {
            file_path = std::filesystem::path(requested_source);
        }

        std::string file_path_string = file_path.string();
        std::string content = read_file(file_path_string);
        return new IncludeResult(file_path_string, content);
    }

    void ReleaseInclude(shaderc_include_result *data) override { delete static_cast<IncludeResult *>(data); }
};

ShaderCompiler::ShaderCompiler() { mCompiler = std::make_unique<shaderc::Compiler>(); }

ShaderCompiler::~ShaderCompiler() = default;

std::vector<uint32_t> ShaderCompiler::compile(
        const std::filesystem::path &source_path, vk::ShaderStageFlagBits stage, ShaderCompileOptions opt
) const {
    shaderc::CompileOptions options = {};
    options.SetTargetSpirv(shaderc_spirv_version_1_3);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

    if (opt.debug)
        options.SetGenerateDebugInfo();

    options.SetIncluder(std::make_unique<ShaderIncluder>());

    for (const auto& macro : opt.macros) {
        options.AddMacroDefinition(macro);
    }

    std::string source = read_file(source_path);

    shaderc_shader_kind kind;
    switch (stage) {
        case vk::ShaderStageFlagBits::eVertex:
            kind = shaderc_vertex_shader;
            break;
        case vk::ShaderStageFlagBits::eTessellationControl:
            kind = shaderc_tess_control_shader;
            break;
        case vk::ShaderStageFlagBits::eTessellationEvaluation:
            kind = shaderc_tess_evaluation_shader;
            break;
        case vk::ShaderStageFlagBits::eGeometry:
            kind = shaderc_geometry_shader;
            break;
        case vk::ShaderStageFlagBits::eFragment:
            kind = shaderc_fragment_shader;
            break;
        case vk::ShaderStageFlagBits::eCompute:
            kind = shaderc_compute_shader;
            break;
        default:
            Logger::fatal("Unknown shader type: " + source_path.string());
    }

    shaderc::PreprocessedSourceCompilationResult preprocessed_result =
            mCompiler->PreprocessGlsl(source, kind, source_path.string().c_str(), options);

    if (preprocessed_result.GetCompilationStatus() != shaderc_compilation_status_success) {
        Logger::fatal(preprocessed_result.GetErrorMessage());
    }

    std::string preprocessed_code = {preprocessed_result.cbegin(), preprocessed_result.cend()};

    if (opt.print)
        Logger::info("Preprocessed source of " + source_path.string() + ": \n" + preprocessed_code);

    if (opt.optimize)
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

    shaderc::SpvCompilationResult module =
            mCompiler->CompileGlslToSpv(preprocessed_code, kind, source_path.string().c_str(), options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        Logger::fatal("Shader compilation failed:\n" + module.GetErrorMessage());
    }

    return {module.begin(), module.end()};
}


UniqueCompiledShaderStage ShaderLoader::loadFromSource(const vk::Device& device, const std::filesystem::path &path_, std::span<std::string> macros) const {
    auto path = root / path_;

    vk::ShaderStageFlagBits stage;
    auto ext = path.extension().string().substr(1);
    if (ext == "vert")
        stage = vk::ShaderStageFlagBits::eVertex;
    else if (ext == "tesc")
        stage = vk::ShaderStageFlagBits::eTessellationControl;
    else if (ext == "tese")
        stage = vk::ShaderStageFlagBits::eTessellationEvaluation;
    else if (ext == "geom")
        stage = vk::ShaderStageFlagBits::eGeometry;
    else if (ext == "frag")
        stage = vk::ShaderStageFlagBits::eFragment;
    else if (ext == "comp")
        stage = vk::ShaderStageFlagBits::eCompute;
    else
        Logger::fatal("Unknown shader type: " + path.string());

    auto code = mCompiler->compile(path, stage, {optimize, debug, print, std::vector(macros.begin(), macros.end())});
    vk::ShaderModuleCreateInfo create_info = {
        .codeSize = code.size() * sizeof(uint32_t),
        .pCode = code.data()
    };
    vk::UniqueShaderModule module = device.createShaderModuleUnique(create_info);
    util::setDebugName(device, *module, path.filename().string());
    return {path.filename().string(), stage, std::move(module)};
}

UniqueCompiledShaderStage ShaderLoader::loadFromBinary(const vk::Device& device, const std::filesystem::path &path_, vk::ShaderStageFlagBits stage) const {
    auto path = root / path_;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Logger::fatal("Error opening file: " + std::filesystem::absolute(path).string());
    }

    file.seekg(0, std::ios::end);
    size_t size_bytes = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint32_t> code;
    code.resize(size_bytes / sizeof(uint32_t));
    file.read(reinterpret_cast<char *>(code.data()), size_bytes);
    file.close();

    vk::ShaderModuleCreateInfo create_info = {
        .codeSize = size_bytes,
        .pCode = reinterpret_cast<const uint32_t *>(code.data())
    };
    vk::UniqueShaderModule module = device.createShaderModuleUnique(create_info);
    util::setDebugName(device, *module, path.filename().string());
    return {path.filename().string(), stage, std::move(module)};
}