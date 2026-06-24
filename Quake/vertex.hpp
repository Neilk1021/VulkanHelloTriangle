//
// Created by Neilf on 6/21/2026.
//

#ifndef QUAKE_VERTEX_HPP
#define QUAKE_VERTEX_HPP
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <vulkan/vulkan_raii.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription(){
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };
    }
    
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        return
        {
            {
                {
                    .location = 0,
                    .binding = 0,
                    .format = vk::Format::eR32G32B32Sfloat,
                    .offset = offsetof(Vertex, pos)
                },
                {
                    .location = 1,
                    .binding = 0,
                    .format = vk::Format::eR32G32B32Sfloat,
                    .offset = offsetof(Vertex, color)
                },
                 {
                     .location = 2,
                     .binding = 0,
                     .format = vk::Format::eR32G32Sfloat,
                     .offset = offsetof(Vertex, texCoord)
                 }
            }
        };
    }

    friend std::ostream & operator << (std::ostream & os, const Vertex & v) {
        os << "Position: " << v.pos.r << ", " << v.pos.g << ", " << v.pos.b << std::endl;
        os << "Color: " << v.color.r << ", " << v.color.g << ", " << v.color.b << std::endl;
        os << "TexCoord: " << v.texCoord.r << ", " << v.texCoord.g << std::endl;
        return os;
    }

    bool operator==(const Vertex & other) const {
        return other.pos == pos && other.color == color && other.texCoord == texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}
#endif //QUAKE_VERTEX_HPP
