//
// Created by Neilf on 6/24/2026.
//

#include "ModelLoader.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <exception>
Model ModelLoader::loadObj(const std::string &filepath) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn, err;

    std::unordered_map<Model::VertexType, Model::IndexType> uniqueVertices {};

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    std::vector<Model::VertexType> vertices;
    std::vector<Model::IndexType> indices;

    for (const auto & shape : shapes) {
        for (const auto & index : shape.mesh.indices) {
             Model::VertexType vertex{
                .pos{
                    attrib.vertices[3* index.vertex_index + 0],
                    attrib.vertices[3* index.vertex_index + 1],
                    attrib.vertices[3* index.vertex_index + 2],
                },
                 .color = {1.0f, 1.0f, 1.0f},
                 .texCoord = {
                     attrib.texcoords[2 * index.texcoord_index +0],
                     1.0f - attrib.texcoords[2 * index.texcoord_index +1]
                 },
            };

            if (!uniqueVertices.contains(vertex)) {
                uniqueVertices[vertex] = static_cast<Model::IndexType>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    return Model{
        .vertices = vertices,
        .indices = indices
    };
}
