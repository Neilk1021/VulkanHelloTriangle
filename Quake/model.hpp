//
// Created by Neilf on 6/24/2026.
//

#ifndef QUAKE_MODEL_HPP
#define QUAKE_MODEL_HPP

#include <vector>

#include "vertex.hpp"


struct Model {
    using VertexType = Vertex;
    using IndexType = uint32_t;

    std::vector<VertexType> vertices;
    std::vector<IndexType> indices;

    size_t vertexBufferSize() const {
        return vertices.size() * sizeof(VertexType);
    }

    size_t indexBufferSize() const {
        return indices.size() * sizeof(IndexType);
    }
};
#endif //QUAKE_MODEL_HPP
