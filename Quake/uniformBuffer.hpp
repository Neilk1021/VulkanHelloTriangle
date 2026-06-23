//
// Created by Neilf on 6/22/2026.
//

#ifndef QUAKE_UNIFORMBUFFER_HPP
#define QUAKE_UNIFORMBUFFER_HPP
struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
#endif //QUAKE_UNIFORMBUFFER_HPP
