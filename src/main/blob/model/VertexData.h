#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

struct VertexData {
    glm::vec3 position{};
    glm::vec3 normal{};

    static std::vector<vk::VertexInputBindingDescription> getBindingDescriptions() {
        std::vector<vk::VertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(VertexData);
        bindingDescriptions[0].inputRate = vk::VertexInputRate::eVertex;
        return bindingDescriptions;
    }

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(2);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(VertexData, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(VertexData, normal);

        return attributeDescriptions;
    }
};
