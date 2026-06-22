//
// Created by Neilf on 6/21/2026.
//

#ifndef QUAKE_VERTEX_HPP
#define QUAKE_VERTEX_HPP
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription(){
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };
    }
    
    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return
        {
            {
                {
                    .location = 0,
                    .binding = 0,
                    .format = vk::Format::eR32G32Sfloat,
                    .offset = offsetof(Vertex, pos)
                },
                 {
                     .location = 1,
                     .binding = 0,
                     .format = vk::Format::eR32G32B32Sfloat,
                     .offset = offsetof(Vertex, color)
                 }
            }
        };
    }
};
#endif //QUAKE_VERTEX_HPP
