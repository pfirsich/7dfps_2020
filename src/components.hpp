#pragma once

#include <string>

#include "ecs.hpp"

namespace comp {
struct Hierarchy {
    ecs::EntityHandle parent;
    ecs::EntityHandle firstChild;
    ecs::EntityHandle prevSibling;
    ecs::EntityHandle nextSibling;

    static void removeParent(ecs::EntityHandle& entity);
    static void setParent(ecs::EntityHandle& entity, ecs::EntityHandle& parent);
};

struct Name {
    std::string value;
};
}
