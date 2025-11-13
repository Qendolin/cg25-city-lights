#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include "../util/static_vector.h"

namespace blob {

    struct VertexData {
        glm::vec4 position{};
        glm::vec4 normal{};

        static util::static_vector<vk::VertexInputBindingDescription, 16> getBindingDescriptions() {
            util::static_vector<vk::VertexInputBindingDescription, 16> bindingDescriptions;
            
            vk::VertexInputBindingDescription bindingDescription;
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(VertexData);
            bindingDescription.inputRate = vk::VertexInputRate::eVertex;
            bindingDescriptions.push_back(bindingDescription);

            return bindingDescriptions;
        }

        static util::static_vector<vk::VertexInputAttributeDescription, 16> getAttributeDescriptions() {
            util::static_vector<vk::VertexInputAttributeDescription, 16> attributeDescriptions;
            attributeDescriptions.resize(2);

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

} // namespace blob
