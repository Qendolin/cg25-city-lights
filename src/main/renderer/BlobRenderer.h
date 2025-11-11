#pragma once

#include <glm/glm.hpp>

#include "../backend/Framebuffer.h"
#include "../backend/Image.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../blob/model/Model.h"
#include "../entity/Camera.h"

class BlobRenderer {
public:
    struct BlobShaderPushConstant {
        glm::mat4 projectionViewModel{1.f};
        glm::mat4 modelMatrix{1.f};
    };

    BlobRenderer() = default;
    ~BlobRenderer() = default;

    void recreate(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
        createPipeline(device, shaderLoader, framebuffer);
    }

    void execute(
            const vk::CommandBuffer &cmd_buf, const Framebuffer &framebuffer, const Camera &camera, const blob::Model &blobModel
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &framebuffer);

    ConfiguredGraphicsPipeline mPipeline;
};
