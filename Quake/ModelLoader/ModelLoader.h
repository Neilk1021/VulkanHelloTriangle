//
// Created by Neilf on 6/24/2026.
//

#ifndef QUAKE_MODELLOADER_H
#define QUAKE_MODELLOADER_H
#include "../model.hpp"

namespace ModelLoader {
    Model loadObj(const std::string& filepath);
}
#endif //QUAKE_MODELLOADER_H
